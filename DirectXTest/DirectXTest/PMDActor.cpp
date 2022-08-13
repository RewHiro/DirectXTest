#include "PMDActor.h"
#include "PMDRenderer.h"
#include "Dx12Wrapper.h"
#include <d3dx12.h>
#include <mmsystem.h>
#include <algorithm>

#pragma comment(lib, "winmm.lib")

using namespace Microsoft::WRL;
using namespace std;
using namespace DirectX;

namespace {
	// �e�N�X�`���̃p�X���Z�p���[�^�[�����ŕ�������
	// @param path �Ώۂ̃p�X������
	// @param slitter ��؂蕶��
	// @return �����O��̕�����y�A
	std::pair<std::string, std::string> SplitFileName
	(
		const std::string& path,
		const char splitter = '*'
	)
	{
		int idx = path.find(splitter);
		std::pair<std::string, std::string> ret;
		ret.first = path.substr(0, idx);
		ret.second = path.substr(idx + 1, path.length() - idx - 1);
		return ret;
	}

	// �t�@�C��������g���q���擾����
	// @param path �Ώۂ̃p�X������
	// @return �g���q
	std::string GetExtension(const std::string& path)
	{
		int idx = path.rfind('.');
		return path.substr(idx + 1, path.length() - idx - 1);
	}

	// ���f���̃p�X�ƃe�N�X�`���̃p�X���獇���p�X�𓾂�
	// @param modelPath �A�v���P�[�V�������猩��pmd���f���̃p�X
	// @param texPath PMD���f�����猩���e�N�X�`���̃p�X
	// @param return �A�v���P�[�V�������猩���e�N�X�`���̃p�X
	std::string GetTexturePathFromModelAndTexPath
	(
		const std::string& modelPath,
		const char* texPath
	)
	{
		// �t�@�C���̃t�H���_��؂��\��/��2��ނ��g�p�����\��������
		// �Ƃ�����������\��/�𓾂���΂����̂ŁA�o����rfind���Ƃ��r����
		// int�^�ɑ�����Ă���̂͌�����Ȃ������ꍇ��rfind��epos(-1->0xffffffff)��Ԃ�����
		int pathIndex1 = modelPath.rfind('/');
		int pathIndex2 = modelPath.rfind('\\');
		auto pathIndex = max(pathIndex1, pathIndex2);
		auto folderPath = modelPath.substr(0, pathIndex + 1);
		return folderPath + texPath;
	}
}

void* PMDActor::Transform::operator new(size_t size)
{
	return _aligned_malloc(size, 16);
}

HRESULT PMDActor::CreateMaterialData()
{
	// �}�e���A���o�b�t�@�[���쐬
	auto materialBuffSize = sizeof(MaterialForHlsl);
	materialBuffSize = (materialBuffSize + 0xff) & ~0xff;

	auto heapprop = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
	auto resdesc = CD3DX12_RESOURCE_DESC::Buffer(materialBuffSize * _materials.size()); // ���������Ȃ����d���Ȃ�

	auto result = _dx12.Device()->CreateCommittedResource
	(
		&heapprop,
		D3D12_HEAP_FLAG_NONE,
		&resdesc,
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(_materialBuff.ReleaseAndGetAddressOf())
	);

	if (FAILED(result))
	{
		assert(SUCCEEDED(result));
		return result;
	}

	// �}�b�v�}�e���A���ɃR�s�[
	char* mapMaterial = nullptr;
	result = _materialBuff->Map(0, nullptr, (void**)&mapMaterial);
	if (FAILED(result))
	{
		assert(SUCCEEDED(result));
		return result;
	}

	for (auto& m : _materials)
	{
		*((MaterialForHlsl*)mapMaterial) = m.material; // �f�[�^�R�s�[
		mapMaterial += materialBuffSize; // ���̃A���C�����g�ʒu�܂Ői�߂�(256�̔{��)
	}

	_materialBuff->Unmap(0, nullptr);

	return S_OK;
}

HRESULT PMDActor::CreateMaterialAndTextureView()
{
	D3D12_DESCRIPTOR_HEAP_DESC materialDescHeapDesc = {};
	materialDescHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	materialDescHeapDesc.NodeMask = 0;
	materialDescHeapDesc.NumDescriptors = _materials.size() * 5; // �}�e���A����x5
	materialDescHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;

	auto result = _dx12.Device()->CreateDescriptorHeap(&materialDescHeapDesc, IID_PPV_ARGS(_materialHeap.ReleaseAndGetAddressOf()));
	if (FAILED(result))
	{
		assert(SUCCEEDED(result));
		return result;
	}

	auto materialBuffSize = sizeof(MaterialForHlsl);
	materialBuffSize = (materialBuffSize + 0xff) & ~0xff;
	D3D12_CONSTANT_BUFFER_VIEW_DESC matCBVDesc = {};
	matCBVDesc.BufferLocation = _materialBuff->GetGPUVirtualAddress();
	matCBVDesc.SizeInBytes = materialBuffSize;

	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING; // ��q
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D; // 2D�e�N�X�`��
	srvDesc.Texture2D.MipLevels = 1; // �~�b�v�}�b�v�͎g�p���Ȃ��̂�1

	CD3DX12_CPU_DESCRIPTOR_HANDLE matDescHeapH(_materialHeap->GetCPUDescriptorHandleForHeapStart());
	auto incSize = _dx12.Device()->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

	for (size_t i = 0; i < _materials.size(); ++i)
	{
		// �}�e���A���p�萔�o�b�t�@�r���[
		_dx12.Device()->CreateConstantBufferView(&matCBVDesc, matDescHeapH);
		matDescHeapH.ptr += incSize;
		matCBVDesc.BufferLocation += materialBuffSize;

		// �V�F�[�_�[���\�[�X�r���[
		if (_textureResources[i] == nullptr)
		{
			srvDesc.Format = _renderer._whiteTex->GetDesc().Format;
			_dx12.Device()->CreateShaderResourceView
			(
				_renderer._whiteTex.Get(),
				&srvDesc,
				matDescHeapH
			);
		}
		else
		{
			srvDesc.Format = _textureResources[i]->GetDesc().Format;
			_dx12.Device()->CreateShaderResourceView
			(
				_textureResources[i].Get(),
				&srvDesc,
				matDescHeapH
			);
		}

		matDescHeapH.Offset(incSize);

		if (_sphResources[i] == nullptr)
		{
			srvDesc.Format = _renderer._whiteTex->GetDesc().Format;
			_dx12.Device()->CreateShaderResourceView
			(
				_renderer._whiteTex.Get(),
				&srvDesc,
				matDescHeapH
			);
		}
		else
		{
			srvDesc.Format = _sphResources[i]->GetDesc().Format;
			_dx12.Device()->CreateShaderResourceView
			(
				_sphResources[i].Get(),
				&srvDesc,
				matDescHeapH
			);
		}

		matDescHeapH.ptr += incSize;

		if (_spaResources[i] == nullptr)
		{
			srvDesc.Format = _renderer._blackTex->GetDesc().Format;
			_dx12.Device()->CreateShaderResourceView
			(
				_renderer._blackTex.Get(),
				&srvDesc,
				matDescHeapH
			);
		}
		else
		{
			srvDesc.Format = _spaResources[i]->GetDesc().Format;
			_dx12.Device()->CreateShaderResourceView
			(
				_spaResources[i].Get(),
				&srvDesc,
				matDescHeapH
			);
		}

		matDescHeapH.ptr += incSize;

		if (_toonResources[i] == nullptr)
		{
			srvDesc.Format = _renderer._gradTex->GetDesc().Format;
			_dx12.Device()->CreateShaderResourceView
			(
				_renderer._gradTex.Get(),
				&srvDesc,
				matDescHeapH
			);
		}
		else
		{
			srvDesc.Format = _toonResources[i]->GetDesc().Format;
			_dx12.Device()->CreateShaderResourceView
			(
				_toonResources[i].Get(),
				&srvDesc,
				matDescHeapH
			);
		}

		matDescHeapH.ptr += incSize;
	}

	return S_OK;
}

HRESULT PMDActor::CreateTransformView()
{
	// GPU�o�b�t�@�쐬
	auto buffSize = sizeof(XMMATRIX) * (1 + _boneMatrices.size());
	buffSize = (buffSize + 0xff) & ~0xff;
	auto heapProp = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
	auto resDesc = CD3DX12_RESOURCE_DESC::Buffer(buffSize);

	auto result = _dx12.Device()->CreateCommittedResource
	(
		&heapProp,
		D3D12_HEAP_FLAG_NONE,
		&resDesc,
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(_transformBuff.ReleaseAndGetAddressOf())
	);

	if (FAILED(result))
	{
		assert(SUCCEEDED(result));
		return result;
	}

	// �}�b�v�ƃR�s�[
	result = _transformBuff->Map(0, nullptr, (void**)&_mappedMatrices);
	if (FAILED(result))
	{
		assert(SUCCEEDED(result));
		return result;
	}

	_mappedMatrices[0] = _transform.world;

	for (auto& bonemotion : _motiondata)
	{
		auto node = _boneNodeTable[bonemotion.first];
		auto& pos = node.startPos;
		auto mat = XMMatrixTranslation(-pos.x, -pos.y, -pos.z)
			* XMMatrixRotationQuaternion(bonemotion.second[0].quaternion)
			* XMMatrixTranslation(pos.x, pos.y, pos.z);
		_boneMatrices[node.boneIdx] = mat;
	}

	RecursiveMatrixMultiply(&_boneNodeTable["�Z���^�["], XMMatrixIdentity());

	copy(_boneMatrices.begin(), _boneMatrices.end(), _mappedMatrices + 1);

	// �r���[�̍쐬
	D3D12_DESCRIPTOR_HEAP_DESC transformDescHeapDesc = {};
	transformDescHeapDesc.NumDescriptors = 1; // �Ƃ����������[���h�ЂƂ�
	transformDescHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	transformDescHeapDesc.NodeMask = 0;

	transformDescHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	result = _dx12.Device()->CreateDescriptorHeap(&transformDescHeapDesc, IID_PPV_ARGS(_transformHeap.ReleaseAndGetAddressOf()));
	if (FAILED(result))
	{
		assert(SUCCEEDED(result));
		return result;
	}

	D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc = {};
	cbvDesc.BufferLocation = _transformBuff->GetGPUVirtualAddress();
	cbvDesc.SizeInBytes = buffSize;
	_dx12.Device()->CreateConstantBufferView(&cbvDesc, _transformHeap->GetCPUDescriptorHandleForHeapStart());

	return S_OK;
}

HRESULT PMDActor::LoadPMDFile(const char* path)
{
	// PMD�w�b�_�[�\����
	struct PMDHeader
	{
		float version; // ��F00 80 3F == 1.00
		char model_name[20]; // ���f����
		char comment[256]; // ���f���R�����g
	};

	PMDHeader pmdheader;

	char signature[3] = {}; // �V�O�l�`��
	FILE* fp = nullptr;
	std::string strModelPath = path;
	fopen_s(&fp, strModelPath.c_str(), "rb");

	if (fp == nullptr)
	{
		// �G���[����
		assert(0);
		return ERROR_FILE_NOT_FOUND;
	}

	fread(signature, sizeof(signature), 1, fp);
	fread(&pmdheader, sizeof(pmdheader), 1, fp);

	constexpr size_t pmdvertex_size = 38; // ���_1������̃T�C�Y
	unsigned int vertNum = 0; // ���_��
	fread(&vertNum, sizeof(vertNum), 1, fp);

	std::vector<unsigned char>vertices(vertNum * pmdvertex_size); // �o�b�t�@�[�̊m��
	fread(vertices.data(), vertices.size(), 1, fp);

	unsigned int indicesNum = 0; // �C���f�b�N�X��
	fread(&indicesNum, sizeof(indicesNum), 1, fp);

	std::vector<unsigned short> indices(indicesNum);
	fread(indices.data(), indices.size() * sizeof(indices[0]), 1, fp);

#pragma pack(1) // ��������1�o�C�g�p�b�L���O�ƂȂ�A�A���C�����g�͔������Ȃ�

	// PMD�}�e���A���\����
	struct PMDMaterial
	{
		DirectX::XMFLOAT3 diffuse; // �f�B�t���[�Y�F
		float alpha; // �f�B�t���[�Y��
		float specularity; // �X�y�L�����̋���(��Z�l)
		DirectX::XMFLOAT3 specular; // �X�y�L�����F
		DirectX::XMFLOAT3 ambient; // �A���r�G���g�F
		unsigned char toonIdx; // �g�D�[���ԍ�
		unsigned char edgeFlg; // �}�e���A�����Ƃ̗֊s���t���O
		// ���ӁF������2�o�C�g�̃p�f�B���O����I�I
		unsigned int indicesNum; // �����̃}�e���A�������蓖�Ă���
								 // �C���f�b�N�X��
		char texFilePath[20]; // �e�N�X�`���t�@�C���p�X+��
	}; // 70�o�C�g�̂͂������A�p�f�B���O���������邽��72�o�C�g�ɂȂ�

#pragma pack() // �p�b�L���O�w�������(�f�t�H���g�ɖ߂�)


	auto heapprop = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
	auto resdesc = CD3DX12_RESOURCE_DESC::Buffer(vertices.size() * sizeof(vertices[0]));

	auto result = _dx12.Device()->CreateCommittedResource
	(
		&heapprop,
		D3D12_HEAP_FLAG_NONE,
		&resdesc,
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(_vb.ReleaseAndGetAddressOf())
	);

	unsigned char* vertMap = nullptr;
	result = _vb->Map(0, nullptr, (void**)&vertMap);
	std::copy(std::begin(vertices), std::end(vertices), vertMap);
	_vb->Unmap(0, nullptr);

	_vbView.BufferLocation = _vb->GetGPUVirtualAddress(); // �o�b�t�@�[�̉��z�A�h���X
	_vbView.SizeInBytes = vertices.size(); // �S�o�C�g��
	_vbView.StrideInBytes = pmdvertex_size; // 1���_������̃o�C�g��


	auto resDescBuf = CD3DX12_RESOURCE_DESC::Buffer(indices.size() * sizeof(indices[0]));

	result = _dx12.Device()->CreateCommittedResource
	(
		&heapprop,
		D3D12_HEAP_FLAG_NONE,
		&resDescBuf,
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(_ib.ReleaseAndGetAddressOf())
	);

	// ������o�b�t�@�[�ɃC���f�b�N�X�f�[�^���R�s�[
	unsigned short* mappedIdx = nullptr;
	result = _ib->Map(0, nullptr, (void**)&mappedIdx);
	std::copy(std::begin(indices), std::end(indices), mappedIdx);
	_ib->Unmap(0, nullptr);

	// �C���f�b�N�X�o�b�t�@�r���[���쐬
	_ibView.BufferLocation = _ib->GetGPUVirtualAddress(); // �o�b�t�@�[�̉��z�A�h���X
	_ibView.Format = DXGI_FORMAT_R16_UINT;
	_ibView.SizeInBytes = indices.size() * sizeof(indices[0]); // �S�o�C�g��

	unsigned int materialNum; // �}�e���A����
	fread(&materialNum, sizeof(materialNum), 1, fp);
	_materials.resize(materialNum);
	_textureResources.resize(materialNum);
	_sphResources.resize(materialNum);
	_spaResources.resize(materialNum);
	_toonResources.resize(materialNum);


	std::vector<PMDMaterial> pmdMaterials(materialNum);
	fread(pmdMaterials.data(), pmdMaterials.size() * sizeof(PMDMaterial), 1, fp); // ��C�ɓǂݍ���

	// �R�s�[
	for (size_t i = 0; i < pmdMaterials.size(); ++i)
	{
		_materials[i].indicesNum = pmdMaterials[i].indicesNum;
		_materials[i].material.diffuse = pmdMaterials[i].diffuse;
		_materials[i].material.alpha = pmdMaterials[i].alpha;
		_materials[i].material.specular = pmdMaterials[i].specular;
		_materials[i].material.specularity = pmdMaterials[i].specularity;
		_materials[i].material.ambient = pmdMaterials[i].ambient;
		_materials[i].additional.toonIdx = pmdMaterials[i].toonIdx;
	}


		// �R�s�[
	for (size_t i = 0; i < pmdMaterials.size(); ++i)
	{
		char toonFilePath[32];
		sprintf_s(toonFilePath, "toon/toon%02d.bmp", pmdMaterials[i].toonIdx + 1);
		_toonResources[i] = _dx12.GetTextureByPath(toonFilePath);

		if (strlen(pmdMaterials[i].texFilePath) == 0)
		{
			_textureResources[i] = nullptr;
			continue;
		}

		string texFileName = pmdMaterials[i].texFilePath;
		string sphFileName = "";
		string spaFileName = "";
		if (count(texFileName.begin(), texFileName.end(), '*') > 0)
		{
			auto namepair = SplitFileName(texFileName);
			if (GetExtension(namepair.first) == "sph")
			{
				texFileName = namepair.second;
				sphFileName = namepair.first;
			}
			else if (GetExtension(namepair.first) == "spa")
			{
				texFileName = namepair.second;
				spaFileName = namepair.first;
			}
			else
			{
				texFileName = namepair.first;
				if (GetExtension(namepair.second) == "sph")
				{
					sphFileName = namepair.second;
				}
				else if (GetExtension(namepair.second) == "spa")
				{
					spaFileName = namepair.second;
				}
			}
		}
		else
		{
			if (GetExtension(pmdMaterials[i].texFilePath) == "sph")
			{
				sphFileName = pmdMaterials[i].texFilePath;
				texFileName = "";
			}
			else if (GetExtension(pmdMaterials[i].texFilePath) == "spa")
			{
				spaFileName = pmdMaterials[i].texFilePath;
				texFileName = "";
			}
			else
			{
				texFileName = pmdMaterials[i].texFilePath;
			}
		}


		if (texFileName != "")
		{
			auto texFilePath = GetTexturePathFromModelAndTexPath(strModelPath, texFileName.c_str());
			_textureResources[i] = _dx12.GetTextureByPath(texFilePath.c_str());
		}
		if (sphFileName != "")
		{
			auto sphFilePath = GetTexturePathFromModelAndTexPath(strModelPath, sphFileName.c_str());
			_sphResources[i] = _dx12.GetTextureByPath(sphFilePath.c_str());
		}
		if (spaFileName != "")
		{
			auto spaFilePath = GetTexturePathFromModelAndTexPath(strModelPath, spaFileName.c_str());
			_spaResources[i] = _dx12.GetTextureByPath(spaFilePath.c_str());
		}

	}

	unsigned short boneNum = 0;
	fread(&boneNum, sizeof(boneNum), 1, fp);

	#pragma pack(1)
	// �ǂݍ��ݗp�{�[���\����
	struct PMDBone
	{
		char boneName[20]; // �{�[����
		unsigned short parentNo; // �e�{�[���ԍ�
		unsigned short nextNo; // ��[�̃{�[���ԍ�
		unsigned char type; // �{�[�����
		unsigned short ikBoneNo; // IK�{�[���ԍ�
		XMFLOAT3 pos; // �{�[���̊�_���W
	};
	#pragma pack()

	std::vector<PMDBone> pmdBones(boneNum);
	fread(pmdBones.data(), sizeof(PMDBone), boneNum, fp);

	// �C���f�b�N�X�Ɩ��O�̑Ή��֌W�\�z�̂��߂ɂ��ƂŎg��
	std::vector<std::string> boneNames(pmdBones.size());

	// �{�[���m�[�h�}�b�v�����
	for (size_t idx = 0; idx < pmdBones.size(); ++idx)
	{
		auto& pb = pmdBones[idx];
		boneNames[idx] = pb.boneName;
		auto& node = _boneNodeTable[pb.boneName];
		node.boneIdx = idx;
		node.startPos = pb.pos;
	}

	// �e�q�֌W���\�z����
	for (auto& pb : pmdBones)
	{
		// �e�C���f�b�N�X���`�F�b�N(���蓾�Ȃ��ԍ��Ȃ��΂�)
		if (pb.parentNo >= pmdBones.size())
		{
			continue;
		}

		auto parentName = boneNames[pb.parentNo];
		_boneNodeTable[parentName].children.emplace_back(&_boneNodeTable[pb.boneName]);
	}

	_boneMatrices.resize(pmdBones.size());
	std::fill(_boneMatrices.begin(), _boneMatrices.end(), XMMatrixIdentity());

	fclose(fp);

	return S_OK;
}

HRESULT PMDActor::LoadVMDFile(const char* path)
{
	FILE* fp = nullptr;
	std::string strModelPath = path;
	fopen_s(&fp, strModelPath.c_str(), "rb");

	fseek(fp, 50, SEEK_SET); // �ŏ���50�o�C�g�͔�΂��Ă���

	unsigned int motionDataNum = 0;
	fread(&motionDataNum, sizeof(motionDataNum), 1, fp);
	
	struct VMDMotionData
	{
		char boneName[15]; // �{�[����
		unsigned int frameNo; // �t���[���ԍ�
		XMFLOAT3 location; // �ʒu
		XMFLOAT4 quaternion; // �N�H�[�^�j�I��(��])
		unsigned char bezier[64]; // [4][4][4] �x�W�F��ԃp�����[�^
	};

	std::vector<VMDMotionData> vmdMotionData(motionDataNum);

	for (auto& motion : vmdMotionData)
	{
		fread(motion.boneName, sizeof(motion.boneName), 1, fp); // �{�[����
		fread(&motion.frameNo, sizeof(motion.frameNo) // �t���[���ԍ�
			+ sizeof(motion.location) // �ʒu(IK�̂Ƃ��Ɏg�p�\��)
			+ sizeof(motion.quaternion) // �N�H�[�^�j�I��
			+ sizeof(motion.bezier) // ��ԃx�W�F�f�[�^
			, 1, fp);

		_duration = std::max<unsigned int>(_duration, motion.frameNo);
	}

	// VMD�̃��[�V�����f�[�^����A���ۂɎg�p���郂�[�V�����e�[�u���֕ϊ�
	for (auto& vmdMotion : vmdMotionData)
	{
		_motiondata[vmdMotion.boneName].emplace_back(
			KeyFrame
			(
				vmdMotion.frameNo
				, XMLoadFloat4(&vmdMotion.quaternion)
				, XMFLOAT2((float)vmdMotion.bezier[3] / 127.0f, (float)vmdMotion.bezier[7] / 127.0f)
				, XMFLOAT2((float)vmdMotion.bezier[11] / 127.0f, (float)vmdMotion.bezier[15] / 127.0f)
			)
		);
	}

	fclose(fp);

	return S_OK;
}

void PMDActor::RecursiveMatrixMultiply(BoneNode* node, const DirectX::XMMATRIX& mat)
{
	_boneMatrices[node->boneIdx] *= mat;

	for (auto& cnode : node->children)
	{
		RecursiveMatrixMultiply(cnode, _boneMatrices[node->boneIdx]);
	}
}

void PMDActor::MotionUpdate()
{
	auto elapsedTime = timeGetTime() - _startTime; // �o�ߎ��Ԃ𑪂�
	unsigned int frameNo = static_cast<unsigned int>(30 * (elapsedTime / 1000.0f));

	if (frameNo > _duration)
	{
		_startTime = timeGetTime();
		frameNo = 0;
	}

	std::fill(_boneMatrices.begin(), _boneMatrices.end(), XMMatrixIdentity());

	// ���[�V�����f�[�^�X�V
	for (auto& bonemotion : _motiondata)
	{
		auto itBoneNode = _boneNodeTable.find(bonemotion.first);

		if (itBoneNode == _boneNodeTable.end())
		{
			continue;
		}

		std::sort(bonemotion.second.begin(), bonemotion.second.end(), [](const KeyFrame& lval, const KeyFrame& rval) {return lval.frameNo <= rval.frameNo; });

		auto& node = itBoneNode->second;

		// ���v������̂�T��
		auto motions = bonemotion.second;
		auto rit = std::find_if(motions.rbegin(), motions.rend(), [frameNo](const KeyFrame& motion) { return motion.frameNo <= frameNo; });

		// ���v������̂��Ȃ���Ώ������΂�
		if (rit == motions.rend())
		{
			continue;
		}

		XMMATRIX rotation;
		auto it = rit.base();

		if (it != motions.end())
		{
			auto t = static_cast<float>(frameNo - rit->frameNo) / static_cast<float>(it->frameNo - rit->frameNo);
			t = GetYFromXOnBezier(t, it->p1, it->p2, 12);
			rotation = XMMatrixRotationQuaternion(XMQuaternionSlerp(rit->quaternion, it->quaternion, t));
		}
		else
		{
			rotation = XMMatrixRotationQuaternion(rit->quaternion);
		}

		auto& pos = node.startPos;
		auto mat = XMMatrixTranslation(-pos.x, -pos.y, -pos.z)
			* rotation
			* XMMatrixTranslation(pos.x, pos.y, pos.z);
		_boneMatrices[node.boneIdx] = mat;
	}

	RecursiveMatrixMultiply(&_boneNodeTable["�Z���^�["], XMMatrixIdentity());

	copy(_boneMatrices.begin(), _boneMatrices.end(), _mappedMatrices + 1);
}

float PMDActor::GetYFromXOnBezier(float x, XMFLOAT2& a, XMFLOAT2& b, uint8_t n)
{
	if (a.x == a.y && b.x == b.y)
	{
		return x; // �v�Z�s�v
	}

	float t = x;
	const float k0 = 1 + 3 * a.x - 3 * b.x; // t^3�̌W��
	const float k1 = 3 * b.x - 6 * a.x; // t^2�̌W��
	const float k2 = 3 * a.x; // t�̌W��

	// �덷�͈͓̔����ǂ����Ɏg�p����萔
	constexpr float epsilon = 0.0005f;

	// t�Ƌߎ��ŋ��߂�
	for (int i = 0; i < n; ++i)
	{
		// f(t)�����߂�
		auto ft = k0 * t * t * t + k1 * t * t + k2 * t - x;

		// �������ʂ�0�ɋ߂�(�덷�͈͓̔�)�Ȃ�ł��؂�
		if (ft <= epsilon && ft >= -epsilon)
		{
			break;
		}

		t -= ft / 2; // ����
	}

	// ���߂���t�͂��łɋ��߂Ă���̂�y���v�Z����
	auto r = 1 - t;
	return t * t * t + 3 * t * t * r * b.y + 3 * t * r * r* a.y;
}

PMDActor::PMDActor(const char* filepath, const char* vmdFilePath, PMDRenderer& renderer):
	_renderer(renderer)
	, _dx12(renderer._dx12)
	, _angle(0.0f)
{
	_transform.world = XMMatrixIdentity();
	LoadPMDFile(filepath);
	LoadVMDFile(vmdFilePath);
	CreateTransformView();
	CreateMaterialData();
	CreateMaterialAndTextureView();
}

PMDActor::~PMDActor()
{
}

PMDActor* PMDActor::Clone()
{
	return nullptr;
}

void PMDActor::Update()
{
	MotionUpdate();
	_angle += 0.03f;
	//_mappedTransform->world = XMMatrixRotationY(_angle);
}

void PMDActor::Draw()
{
	_dx12.CommandList()->IASetVertexBuffers(0, 1, &_vbView);
	_dx12.CommandList()->IASetIndexBuffer(&_ibView);

	ID3D12DescriptorHeap* transheaps[] = { _transformHeap.Get() };
	_dx12.CommandList()->SetDescriptorHeaps(1, transheaps);
	_dx12.CommandList()->SetGraphicsRootDescriptorTable(1, _transformHeap->GetGPUDescriptorHandleForHeapStart());

	ID3D12DescriptorHeap* mdh[] = { _materialHeap.Get() };
	_dx12.CommandList()->SetDescriptorHeaps(1, mdh);

	auto materialH = _materialHeap->GetGPUDescriptorHandleForHeapStart(); // �q�[�v�擪
	unsigned int idxOffset = 0; // �ŏ��̓I�t�Z�b�g�Ȃ�

	auto cbvsrvIncSize = _dx12.Device()->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV) * 5;

	for (auto& m : _materials)
	{
		_dx12.CommandList()->SetGraphicsRootDescriptorTable(2, materialH);
		_dx12.CommandList()->DrawIndexedInstanced(m.indicesNum, 1, idxOffset, 0, 0);

		// �q�[�v�|�C���^�[�ƃC���f�b�N�X�����ɐi�߂�
		materialH.ptr += cbvsrvIncSize;
		idxOffset += m.indicesNum;
	}
}

void PMDActor::PlayAnimation()
{
	_startTime = timeGetTime();
}


