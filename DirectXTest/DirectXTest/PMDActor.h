#pragma once

#include <d3d12.h>
#include <DirectXMath.h>
#include <vector>
#include <string>
#include <wrl.h>
#include <map>
#include <unordered_map>

class Dx12Wrapper;
class PMDRenderer;

class PMDActor
{
	friend PMDRenderer;

	PMDRenderer& _renderer;
	Dx12Wrapper& _dx12;
	template<typename T>
	using ComPtr = Microsoft::WRL::ComPtr<T>;

	// ���_�֘A
	ComPtr<ID3D12Resource> _vb = nullptr;
	ComPtr<ID3D12Resource> _ib = nullptr;
	D3D12_VERTEX_BUFFER_VIEW _vbView = {};
	D3D12_INDEX_BUFFER_VIEW _ibView = {};

	ComPtr<ID3D12Resource> _transformMat = nullptr; // ���W�ϊ��s��(���̓��[���h�̂�)
	ComPtr<ID3D12DescriptorHeap> _transformHeap = nullptr; // ���W�ϊ��q�[�v

	// �V�F�[�_�[���ɓ�������}�e���A���f�[�^
	struct MaterialForHlsl
	{
		DirectX::XMFLOAT3 diffuse; // �f�B�t���[�Y�F
		float alpha; // �f�B�t���[�Y��
		DirectX::XMFLOAT3 specular; // �X�y�L�����F
		float specularity; // �X�y�L�����̋���(��Z�l)
		DirectX::XMFLOAT3 ambient; // �A���r�G���g�F
	};

	// ����ȊO�̃}�e���A���f�[�^
	struct AdditionalMaterial
	{
		std::string texPath; // �e�N�X�`���t�@�C���|�X
		int toonIdx; // �g�D�[���ԍ�
		bool edgeFlg; // �}�e���A�����Ƃ̗֊s���t���O
	};

	struct Material
	{
		unsigned int indicesNum; // �C���f�b�N�X��
		MaterialForHlsl material;
		AdditionalMaterial additional;
	};

	struct Transform 
	{
		// �����Ɏ����Ă�XMMATRIX�����o��16�o�C�g�A���C�����g�ł��邽��
		// Transform��new����ۂɂ�16�o�C�g���E�Ɋm�ۂ���
		void* operator new(size_t size);
		DirectX::XMMATRIX world;
	};

	struct BoneNode
	{
		int boneIdx; // �{�[���C���f�b�N�X
		DirectX::XMFLOAT3 startPos; // �{�[����_(��]�̒��S)
		DirectX::XMFLOAT3 endPos; // �{�[����[�_(���ۂ̃X�L�j���O�ɂ͗��p���Ȃ�)
		std::vector<BoneNode*> children; // �q�m�[�h
	};

	// ���[�V�����\����
	struct KeyFrame
	{
		unsigned int frameNo; // �A�j���[�V�����J�n����̃t���[����
		DirectX::XMVECTOR quaternion; // �N�H�[�^�j�I��
		DirectX::XMFLOAT2 p1, p2; // �x�W�F�Ȑ��̒��ԃR���g���[���|�C���g

		KeyFrame(unsigned int fno, const DirectX::XMVECTOR& q, const DirectX::XMFLOAT2& ip1, const DirectX::XMFLOAT2& ip2)
			: frameNo(fno)
			, quaternion(q) 
			, p1(ip1)
			, p2(ip2)
		{}
	};

	Transform _transform;
	Transform* _mappedTransform = nullptr;
	DirectX::XMMATRIX* _mappedMatrices = nullptr;
	ComPtr<ID3D12Resource> _transformBuff = nullptr;

	// �}�e���A���֘A
	std::vector<Material> _materials;
	ComPtr<ID3D12Resource> _materialBuff = nullptr;
	std::vector<ComPtr<ID3D12Resource>> _textureResources;
	std::vector<ComPtr<ID3D12Resource>> _sphResources;
	std::vector<ComPtr<ID3D12Resource>> _spaResources;
	std::vector<ComPtr<ID3D12Resource>> _toonResources;

	std::vector<DirectX::XMMATRIX> _boneMatrices;
	std::map<std::string, BoneNode> _boneNodeTable;
	std::unordered_map<std::string, std::vector<KeyFrame>> _motiondata;

	// �ǂݍ��񂾃}�e���A�������ƂɃ}�e���A���o�b�t�@���쐬
	HRESULT CreateMaterialData();

	ComPtr<ID3D12DescriptorHeap> _materialHeap = nullptr; // �}�e���A���q�[�v(5�Ԃ�)
	// �}�e���A��&�e�N�X�`���r���[���쐬
	HRESULT CreateMaterialAndTextureView();

	// ���W�ϊ��p�r���[�̐���
	HRESULT CreateTransformView();

	// PMD�t�@�C���̃��[�h
	HRESULT LoadPMDFile(const char* path);

	// VMD�t�@�C���̃��[�h
	HRESULT LoadVMDFile(const char* path);

	void RecursiveMatrixMultiply(BoneNode* node, const DirectX::XMMATRIX& mat);

	void MotionUpdate();

	float GetYFromXOnBezier(float x, DirectX::XMFLOAT2& a, DirectX::XMFLOAT2& b, uint8_t n);

	float _angle; // �e�X�g�pY����]
	DWORD _startTime; // �A�j���[�V�����J�n���̃~���b
	DWORD _duration;

public:
	PMDActor(const char* filepath, const char* vmdFilePath, PMDRenderer& renderer);
	~PMDActor();

	// �N���[���͒��_����у}�e���A���͋��ʂ̃o�b�t�@������悤�ɂ���
	PMDActor* Clone();
	void Update();
	void Draw();
	void PlayAnimation();
};