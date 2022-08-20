#include "PMDActor.h"
#include "PMDRenderer.h"
#include "Dx12Wrapper.h"
#include <d3dx12.h>
#include <mmsystem.h>
#include <algorithm>
#include <sstream>
#include <array>

#pragma comment(lib, "winmm.lib")

using namespace Microsoft::WRL;
using namespace std;
using namespace DirectX;

// �덷�͈͓̔����ǂ����Ɏg�p����萔
constexpr float epsilon = 0.0005f;

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

	// �{�[�����
	enum class BoneType
	{
		Rotation // ��]
		, RotAndMode // ��]&�ړ�
		, IK // IK
		, Undefined // ����`
		, IKChild // IK�e���{�[��
		, RotationChild // ��]�e���{�[��
		, IKDestination // IK�ڑ���
		, Invisible // �����Ȃ��{�[��
	};
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

	fread(&_indexNum, sizeof(_indexNum), 1, fp);

	std::vector<unsigned short> indices(_indexNum);
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

	_boneNameArray.resize(pmdBones.size());
	_boneNodeAddressArray.resize(pmdBones.size());

	// �{�[���m�[�h�}�b�v�����
	for (size_t idx = 0; idx < pmdBones.size(); ++idx)
	{
		auto& pb = pmdBones[idx];
		boneNames[idx] = pb.boneName;
		auto& node = _boneNodeTable[pb.boneName];
		node.boneIdx = idx;
		node.startPos = pb.pos;
		_boneNameArray[idx] = pb.boneName;
		_boneNodeAddressArray[idx] = &node;

		string boneName = pb.boneName;
		if (boneName.find("�Ђ�") != string::npos)
		{
			_knessIdxes.emplace_back(idx);
		}
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

	uint16_t ikNum = 0;
	fread(&ikNum, sizeof(ikNum), 1, fp);

	_ikData.resize(ikNum);

	for (auto& ik : _ikData)
	{
		fread(&ik.boneIdx, sizeof(ik.boneIdx), 1, fp);
		fread(&ik.targetIdx, sizeof(ik.targetIdx), 1, fp);

		uint8_t chainLen; // �Ԃ̃m�[�h���������邩
		fread(&chainLen, sizeof(chainLen), 1, fp);

		ik.nodeIdxes.resize(chainLen);
		fread(&ik.iterations, sizeof(ik.iterations), 1, fp);
		fread(&ik.limit, sizeof(ik.limit), 1, fp);

		if (chainLen == 0)
		{
			continue; // �Ԃ̃m�[�h����0�Ȃ�΂����ŏI���
		}

		fread(ik.nodeIdxes.data(), sizeof(ik.nodeIdxes[0]), chainLen, fp);
	}

	fclose(fp);

	// IK�f�o�b�O�p
	auto getNameFromIdx = [&](uint16_t idx)->string
	{
		auto it = find_if(_boneNodeTable.begin(), _boneNodeTable.end(), [idx](const std::pair<string, BoneNode>& obj) {return obj.second.boneIdx == idx; });

		if (it != _boneNodeTable.end())
		{
			return it->first;
		}
		else
		{
			return "";
		}
	};

	for (auto& ik : _ikData)
	{
		std::ostringstream oss;
		oss << "IK�{�[���ԍ�=" << ik.boneIdx << ":" << getNameFromIdx(ik.boneIdx) << endl;

		for (auto& node : ik.nodeIdxes)
		{
			oss << "\t�m�[�h�{�[��=" << node << ":" << getNameFromIdx(node) << endl;
		}

		OutputDebugStringA(oss.str().c_str());
	}

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
				, vmdMotion.location
				, XMFLOAT2((float)vmdMotion.bezier[3] / 127.0f, (float)vmdMotion.bezier[7] / 127.0f)
				, XMFLOAT2((float)vmdMotion.bezier[11] / 127.0f, (float)vmdMotion.bezier[15] / 127.0f)
			)
		);
	}

	#pragma pack(1)
	// �\��f�[�^(���_���[�t�f�[�^)
	struct VMDMorph
	{
		char name[15]; // ���O(�p�f�B���O���Ă��܂�)
		uint32_t frameNo; // �t���[���ԍ�
		float weight; // �E�F�C�g(0.0f�`1.0f)
	}; // �S����23�o�C�g�Ȃ̂�#pragma pack�œǂ�
	#pragma pack()

	uint32_t morphCount = 0;
	fread(&morphCount, sizeof(morphCount), 1, fp);

	std::vector<VMDMorph> morphs(morphCount);
	fread(morphs.data(), sizeof(VMDMorph), morphCount, fp);


	#pragma pack(1)
	// �J����
	struct VMDCamera
	{
		uint32_t frameNo; // �t���[���ԍ�
		float distance; // ����
		XMFLOAT3 pos; // ���W
		XMFLOAT3 eulerAngle; // �I�C���[�p
		uint8_t Interpolation[24]; // ���
		uint32_t fov; // ���E�p
		uint8_t persFlg; // �p�[�X�t���OON / OFF
	}; // 61�o�C�g
	#pragma pack()

	uint32_t vmdCameraCount = 0;
	fread(&vmdCameraCount, sizeof(vmdCameraCount), 1, fp);

	std::vector<VMDCamera> cameraData(vmdCameraCount);
	fread(cameraData.data(), sizeof(VMDCamera), vmdCameraCount, fp);


	// ���C�g�Ɩ��f�[�^
	struct VMDLight
	{
		uint32_t frameNo; // �t���[���ԍ�
		XMFLOAT3 rgb; // ���C�g�F
		XMFLOAT3 vec; // �����x�N�g��(���s����)
	};

	uint32_t vmdLightCount = 0;
	fread(&vmdLightCount, sizeof(vmdLightCount), 1, fp);

	std::vector<VMDLight> lights(vmdLightCount);
	fread(lights.data(), sizeof(VMDLight), vmdLightCount, fp);


	#pragma pack(1)
	// �Z���t�e�f�[�^
	struct VMDSelfShadow
	{
		uint32_t frameNo; // �t���[���ԍ�
		uint8_t mode; // �e���[�h(0�F�e�Ȃ��A1�F���[�h1�A2�F���[�h2)
		float distance; // ����
	};
	#pragma pack()

	uint32_t selfShadowCount = 0;
	fread(&selfShadowCount, sizeof(selfShadowCount), 1, fp);

	std::vector<VMDSelfShadow> selfShadowData(selfShadowCount);
	fread(selfShadowData.data(), sizeof(VMDSelfShadow), selfShadowCount, fp);

	// IK�I�� / �I�t�̐؂�ւ�萔
	uint32_t ikSwitchCount = 0;
	fread(&ikSwitchCount, sizeof(ikSwitchCount), 1, fp);
	_ikEnableData.resize(ikSwitchCount);

	for (auto& ikEnable : _ikEnableData)
	{
		// �L�[�t���[�����Ȃ̂ł܂��̓t���[���ԍ��ǂݍ���
		fread(&ikEnable.frameNo, sizeof(ikEnable.frameNo), 1, fp);

		// ���ɉ��t���O�����邪�A����͎g�p���Ȃ�����
		// 1�o�C�g�V�[�N�ł����܂�Ȃ�
		uint8_t visibleFlg = 0;
		fread(&visibleFlg, sizeof(visibleFlg), 1, fp);

		// �Ώۃ{�[�����ǂݍ���
		uint32_t ikBoneCount = 0;
		fread(&ikBoneCount, sizeof(ikBoneCount), 1, fp);

		// ���[�v�����O��ON / OFF�����擾
		for (size_t i = 0; i < ikBoneCount; ++i)
		{
			char ikBoneName[20];
			fread(&ikBoneName, _countof(ikBoneName), 1, fp);

			uint8_t flg = 0;
			fread(&flg, sizeof(flg), 1, fp);

			ikEnable.ikEnableTable[ikBoneName] = flg;
		}
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

		XMMATRIX rotation = XMMatrixIdentity();
		XMVECTOR offset = XMLoadFloat3(&rit->offset);
		auto it = rit.base();

		if (it != motions.end())
		{
			auto t = static_cast<float>(frameNo - rit->frameNo) / static_cast<float>(it->frameNo - rit->frameNo);
			t = GetYFromXOnBezier(t, it->p1, it->p2, 12);
			rotation = XMMatrixRotationQuaternion(XMQuaternionSlerp(rit->quaternion, it->quaternion, t));
			offset = XMVectorLerp(offset, XMLoadFloat3(&it->offset), t);
		}
		else
		{
			rotation = XMMatrixRotationQuaternion(rit->quaternion);
		}

		auto& pos = node.startPos;
		auto mat = XMMatrixTranslation(-pos.x, -pos.y, -pos.z)
			* rotation
			* XMMatrixTranslation(pos.x, pos.y, pos.z);
		_boneMatrices[node.boneIdx] = mat * XMMatrixTranslationFromVector(offset);
	}

	RecursiveMatrixMultiply(&_boneNodeTable["�Z���^�["], XMMatrixIdentity());

	IKSolve(frameNo);

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
	return t * t * t + 3 * t * t * r * b.y + 3 * t * r * r * a.y;
}

void PMDActor::SolveCCDIK(const PMDIK& ik)
{
	// IK�\���_��ۑ�
	std::vector<XMVECTOR> bonePositions;

	// �^�[�Q�b�g
	auto targetBoneNode = _boneNodeAddressArray[ik.boneIdx];
	auto targetOriginPos = XMLoadFloat3(&targetBoneNode->startPos);

	auto parentMat = _boneMatrices[_boneNodeAddressArray[ik.boneIdx]->ikParentBone];

	XMVECTOR det;
	auto invParentMat = XMMatrixInverse(&det, parentMat);
	auto targetNextPos = XMVector3Transform(targetOriginPos, _boneMatrices[ik.boneIdx] * invParentMat);

	// ���[�m�[�h
	auto endPos = XMLoadFloat3(&_boneNodeAddressArray[ik.targetIdx]->startPos);

	// ���ԃm�[�h(���[�g���܂�)
	for (auto& cidx : ik.nodeIdxes)
	{
		bonePositions.push_back(XMLoadFloat3(&_boneNodeAddressArray[cidx]->startPos));
	}

	std::vector<XMMATRIX> mats(bonePositions.size());
	fill(mats.begin(), mats.end(), XMMatrixIdentity());

	auto ikLimit = ik.limit * XM_PI;

	// ik�ɐݒ肳��Ă��鎎�s�񐔂����J��Ԃ�
	for (int c = 0; c < ik.iterations; ++c)
	{
		// �^�[�Q�b�g�Ɩ��[���قڈ�v�����甲����
		if (XMVector3Length(XMVectorSubtract(endPos, targetNextPos)).m128_f32[0] <= epsilon)
		{
			break;
		}

		// ���ꂼ��̃{�[���������̂ڂ�Ȃ���
		// �p�x�����Ɉ���������Ȃ��悤�ɋȂ��Ă���

		// bonePositions�́ACCD-IK�ɂ�����e�m�[�h�̍��W��
		// �x�N�^�z��ɂ������̂ł�
		for (size_t bidx = 0; bidx < bonePositions.size(); ++bidx)
		{
			const auto& pos = bonePositions[bidx];

			// �Ώۃm�[�h���疖�[�m�[�h�܂ł�
			// �Ώۃm�[�h����^�[�Q�b�g�܂ł̃x�N�g���쐬
			auto vecToEnd = XMVectorSubtract(endPos, pos); // ���[��
			auto vecToTarget = XMVectorSubtract(targetNextPos, pos); // �^�[�Q�b�g��

			// �������K��
			vecToEnd = XMVector3Normalize(vecToEnd);
			vecToTarget = XMVector3Normalize(vecToTarget);

			// �قړ����x�N�g���ɂȂ��Ă��܂����ꍇ��
			// �O�ςł��Ȃ����ߎ��̃{�[���Ɉ����n��
			if (XMVector3Length(XMVectorSubtract(vecToEnd, vecToTarget)).m128_f32[0] <= epsilon)
			{
				continue;
			}

			// �O�όv�Z����ъp�x�v�Z
			auto cross = XMVector3Normalize(XMVector3Cross(vecToEnd, vecToTarget)); // ���ɂȂ�

			// �֗��Ȋ֐��������g��cos(���ϒl)�Ȃ̂�
			// 0�`90����0�`-90���̋�ʂ��Ȃ�
			float angle = XMVector3AngleBetweenVectors(vecToEnd, vecToTarget).m128_f32[0];

			// ��]���E�𒴂��Ă��܂����Ƃ��͌��E�l�ɕ␳
			angle = min(angle, ikLimit);
			XMMATRIX rot = XMMatrixRotationAxis(cross, angle); // ��]�s��쐬

			// ���_���S�ł͂Ȃ�pos���S�ɉ�]
			auto mat = XMMatrixTranslationFromVector(-pos)
				* rot
				* XMMatrixTranslationFromVector(pos);

			// ��]�s���ێ����Ă���(��Z�ŉ�]�d�ˊ|��������Ă���)
			mats[bidx] *= mat;

			// �ΏۂƂȂ�_�����ׂĉ�]������(���݂̓_���猩�Ė��[������])
			// �Ȃ��A��������]������K�v�͂Ȃ�
			for (int idx = bidx - 1; idx >= 0; --idx)
			{
				bonePositions[idx] = XMVector3Transform(bonePositions[idx], mat);
			}

			endPos = XMVector3Transform(endPos, mat);

			// ���������ɋ߂��Ȃ��Ă����烋�[�v�𔲂���
			if (XMVector3Length(XMVectorSubtract(endPos, targetNextPos)).m128_f32[0] <= epsilon)
			{
				break;
			}
		}
	}

	int idx = 0;
	for (auto& cidx : ik.nodeIdxes)
	{
		_boneMatrices[cidx] = mats[idx];
		++idx;
	}

	auto rootNode = _boneNodeAddressArray[ik.nodeIdxes.back()];
	RecursiveMatrixMultiply(rootNode, parentMat);
}

void PMDActor::SolveCosineIK(const PMDIK& ik)
{
	// IK�\���_��ۑ�
	std::vector<XMVECTOR> positions;

	// IK�̂��ꂼ��̃{�[���Ԃ̋�����ۑ�
	std::array<float, 2> edgeLens;

	// �^�[�Q�b�g(���[�{�[���ł͂Ȃ��A���[�{�[�����߂Â��ڕW�{�[���̍��W���擾)
	auto& targetNode = _boneNodeAddressArray[ik.boneIdx];
	auto targetPos = XMVector3Transform(XMLoadFloat3(&targetNode->startPos), _boneMatrices[ik.boneIdx]);

	// IK�`�F�[�����t���Ȃ̂ŁA�t�ɕ��Ԃ悤�ɂ��Ă���

	// ���[�{�[��
	auto endNode = _boneNodeAddressArray[ik.targetIdx];

	positions.emplace_back(XMLoadFloat3(&endNode->startPos));

	// ���Ԃ���у��[�g�{�[��
	for (auto& chainBoneIdx : ik.nodeIdxes)
	{
		auto boneNode = _boneNodeAddressArray[chainBoneIdx];
		positions.emplace_back(XMLoadFloat3(&boneNode->startPos));
	}

	// �킩��Â炢�̂ŋt�ɂ���
	// ���Ȃ��l�͂��̂܂܌v�Z���Ă��܂�Ȃ�
	reverse(positions.begin(), positions.end());

	// ���̒����𑪂��Ă���
	edgeLens[0] = XMVector3Length(XMVectorSubtract(positions[1], positions[0])).m128_f32[0];
	edgeLens[1] = XMVector3Length(XMVectorSubtract(positions[2], positions[1])).m128_f32[0];

	// ���[�g�{�[�����W�ϊ�(�t���ɂȂ��Ă��邽�ߎg�p����C���f�b�N�X�ɒ���)
	positions[0] = XMVector3Transform(positions[0], _boneMatrices[ik.nodeIdxes[1]]);

	// �^�񒆂͎����v�Z�����̂Ōv�Z���Ȃ�

	// ��[�{�[��
	positions[2] = XMVector3Transform(positions[2], _boneMatrices[ik.boneIdx]);

	// ���[�g�����[�ւ̃x�N�g��������Ă���
	auto linearVec = XMVectorSubtract(positions[2], positions[0]);

	float A = XMVector3Length(linearVec).m128_f32[0];
	float B = edgeLens[0];
	float C = edgeLens[1];

	linearVec = XMVector3Normalize(linearVec);

	// ���[�g����^�񒆂ւ̊p�x�v�Z
	float theta1 = acosf((A * A + B * B - C * C) / (2 * A * B));

	// �^�񒆂���^�[�Q�b�g�ւ̊p�x�v�Z
	float theta2 = acosf((B * B + C * C - A * A) / (2 * B * C));

	// �u���v�����߂�
	// �����^�񒆂��u�Ђ��v�ł������ꍇ�ɂ͋����I��X���Ƃ���
	XMVECTOR axis;
	if (find(_knessIdxes.begin(), _knessIdxes.end(), ik.nodeIdxes[0]) == _knessIdxes.end())
	{
		auto vm = XMVector3Normalize(XMVectorSubtract(positions[2], positions[0]));
		auto vt = XMVector3Normalize(XMVectorSubtract(targetPos, positions[0]));
		axis = XMVector3Cross(vt, vm);
	}
	else
	{
		auto right = XMFLOAT3(1, 0, 0);
		axis = XMLoadFloat3(&right);
	}

	// ���ӓ_�FIK�`�F�[���̓��[�g�Ɍ������Ă��琔����邽��1�����[�g�ɋ߂�
	auto mat1 = XMMatrixTranslationFromVector(-positions[0]);
	mat1 *= XMMatrixRotationAxis(axis, theta1);
	mat1 *= XMMatrixTranslationFromVector(positions[0]);

	auto mat2 = XMMatrixTranslationFromVector(-positions[1]);
	mat2 *= XMMatrixRotationAxis(axis, theta2-XM_PI);
	mat2 *= XMMatrixTranslationFromVector(positions[1]);

	_boneMatrices[ik.nodeIdxes[1]] *= mat1;
	_boneMatrices[ik.nodeIdxes[0]] = mat2 * _boneMatrices[ik.nodeIdxes[1]];
	_boneMatrices[ik.targetIdx] = _boneMatrices[ik.nodeIdxes[0]];

}

void PMDActor::SolveLookAt(const PMDIK& ik)
{
	// ���̊֐��ɗ������_�Ńm�[�h��1�����Ȃ��A
	// �`�F�[���ɓ����Ă���m�[�h�ԍ���IK�̃��[�g�m�[�h�̂��̂Ȃ̂ŁA
	// ���̃��[�g�m�[�h���疖�[�Ɍ������x�N�g�����l����
	auto rootNode = _boneNodeAddressArray[ik.nodeIdxes[0]];
	auto targetNode = _boneNodeAddressArray[ik.targetIdx];

	auto opos1 = XMLoadFloat3(&rootNode->startPos);
	auto tpos1 = XMLoadFloat3(&targetNode->startPos);

	auto opos2 = XMVector3TransformCoord(opos1, _boneMatrices[ik.nodeIdxes[0]]);
	auto tpos2 = XMVector3TransformCoord(tpos1, _boneMatrices[ik.boneIdx]);

	auto originVec = XMVectorSubtract(tpos1, opos1);
	auto targetVec = XMVectorSubtract(tpos2, opos2);

	originVec = XMVector3Normalize(originVec);
	targetVec = XMVector3Normalize(targetVec);

	XMMATRIX mat = XMMatrixTranslationFromVector(-opos2)
		* LookAtMatrix(originVec, targetVec, XMFLOAT3(0, 1, 0), XMFLOAT3(1, 0, 0))
		* XMMatrixTranslationFromVector(opos2);

	_boneMatrices[ik.nodeIdxes[0]] = mat;
}

void PMDActor::IKSolve(unsigned int  frameNo)
{

	// �O�ɂ��s�����悤�ɁAIK�I�� / �I�t�����t���[���ԍ��ŋt���猟��
	auto it = find_if(_ikEnableData.rbegin(), _ikEnableData.rend(), [frameNo](const VMDIKEnable& ikenable) {return ikenable.frameNo <= frameNo; });

	for (auto& ik : _ikData)
	{

		if (it != _ikEnableData.rend())
		{
			auto ikEnableIt = it->ikEnableTable.find(_boneNameArray[ik.boneIdx]);

			if (ikEnableIt != it->ikEnableTable.end())
			{
				if (!ikEnableIt->second)
				{
					continue;
				}
			}
		}

		auto childrenNodesCount = ik.nodeIdxes.size();

		switch (childrenNodesCount)
		{
		case 0: // �Ԃ̃{�[������0(���肦�Ȃ�)
			assert(0);
			continue;
		case 1: // �Ԃ̃{�[������1�̂Ƃ���LookAt
			SolveLookAt(ik);
			break;
		case 2: // �Ԃ̃{�[������2�̂Ƃ��͗]���藝IK
			SolveCosineIK(ik);
			break;
		default:  // 3�ȏ�̂Ƃ���CCD-IK
			SolveCCDIK(ik);
			break;
		}
	}
}

DirectX::XMMATRIX PMDActor::LookAtMatrix(const DirectX::XMVECTOR& lookat, const DirectX::XMFLOAT3& up, const DirectX::XMFLOAT3& right)
{
	// ��������������(z��)
	XMVECTOR vz = lookat;

	// (���������������������������Ƃ���)����y���x�N�g��
	XMVECTOR vy = XMVector3Normalize(XMLoadFloat3(&up));

	// (���������������������������Ƃ���)y��
	XMVECTOR vx = XMVector3Normalize(XMVector3Cross(vy, vz));
	vy = XMVector3Normalize(XMVector3Cross(vz, vx));

	// LookAt��up�����������������Ă�����right����ɂ��č�蒼��
	if (std::abs(XMVector3Dot(vy, vz).m128_f32[0]) == 1.0f)
	{
		// ����x�������`
		vx = XMVector3Normalize(XMLoadFloat3(&right));

		// �������������������������Ƃ���Y�����v�Z
		vy = XMVector3Normalize(XMVector3Cross(vz, vx));

		// �^��x�����v�Z
		vx = XMVector3Normalize(XMVector3Cross(vy, vz));
	}

	XMMATRIX ret = XMMatrixIdentity();
	ret.r[0] = vx;
	ret.r[1] = vy;
	ret.r[2] = vz;
	return ret;
}

DirectX::XMMATRIX PMDActor::LookAtMatrix(const DirectX::XMVECTOR& origin, const DirectX::XMVECTOR& lookat, const DirectX::XMFLOAT3& up, const DirectX::XMFLOAT3& right)
{
	return
		XMMatrixTranspose(LookAtMatrix(origin, up, right))
		* LookAtMatrix(lookat, up, right)
		;
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

void PMDActor::Draw(bool isShadow)
{

	_dx12.CommandList()->IASetVertexBuffers(0, 1, &_vbView);
	_dx12.CommandList()->IASetIndexBuffer(&_ibView);

	ID3D12DescriptorHeap* transheaps[] = { _transformHeap.Get() };
	_dx12.CommandList()->SetDescriptorHeaps(1, transheaps);
	_dx12.CommandList()->SetGraphicsRootDescriptorTable(1, _transformHeap->GetGPUDescriptorHandleForHeapStart());

	if (isShadow)
	{
		_dx12.CommandList()->DrawIndexedInstanced(_indexNum, 1, 0, 0, 0);
	}

	else
	{
		ID3D12DescriptorHeap* mdh[] = { _materialHeap.Get() };
		_dx12.CommandList()->SetDescriptorHeaps(1, mdh);

		auto materialH = _materialHeap->GetGPUDescriptorHandleForHeapStart(); // �q�[�v�擪
		unsigned int idxOffset = 0; // �ŏ��̓I�t�Z�b�g�Ȃ�

		auto cbvsrvIncSize = _dx12.Device()->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV) * 5;

		for (auto& m : _materials)
		{
			_dx12.CommandList()->SetGraphicsRootDescriptorTable(2, materialH);
			_dx12.CommandList()->DrawIndexedInstanced(m.indicesNum, 2, idxOffset, 0, 0);

			// �q�[�v�|�C���^�[�ƃC���f�b�N�X�����ɐi�߂�
			materialH.ptr += cbvsrvIncSize;
			idxOffset += m.indicesNum;
		}
	}
}

void PMDActor::PlayAnimation()
{
	_startTime = timeGetTime();
}


