#include <Windows.h>
#include <tchar.h>
#include <d3d12.h>
#include <dxgi1_6.h>
#include <array>
#include <vector>
#include <DirectXMath.h>
#include <d3dcompiler.h>
#include <DirectXTex.h>
#ifdef _DEBUG
#include <iostream>
#endif

#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3dcompiler.lib")
#pragma comment(lib, "DirectXTex.lib")

using namespace std;

// @brief �R���\�[����ʂɃt�H�[�}�b�g�t���������\��
// @param format �t�H�[�}�b�g(%d�Ƃ�%f�Ƃ���)
// @param �ϒ�����
// @remarks ���̊֐��̓f�o�b�O�p�ł��B�f�o�b�O���ɂ������삵�܂���
void DebugOutputFormatString(const char* format, ...) 
{
#ifdef _DEBUG
	va_list valist;
	va_start(valist, format);
	printf(format, valist);
	va_end(valist);
#endif
}

LRESULT WindowProcedure(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
{
	// �E�B���h�E���j�����ꂽ��Ă΂��
	if (msg == WM_DESTROY)
	{
		PostQuitMessage(0); //OS�ɑ΂��āu�������̃A�v���͏I���v�Ɠ`����
		return 0;
	}
	return DefWindowProc(hwnd, msg, wparam, lparam); // ����̏����𕲂�
}

void EnableDebugLayer()
{
	ID3D12Debug* debugLayer = nullptr;
	auto result = D3D12GetDebugInterface(IID_PPV_ARGS(&debugLayer));
	debugLayer->EnableDebugLayer(); // �f�o�b�O���C���[��L��������
	debugLayer->Release(); // �L����������C���^�[�t�F�C�X���������
}

#ifdef _DEBUG
int main()
{
#else
int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSRT, int)
#endif
	//DebugOutputFormatString("Show window test.");
	//getchar();


	WNDCLASSEX w = {};
	w.cbSize = sizeof(WNDCLASSEX);
	w.lpfnWndProc = (WNDPROC)WindowProcedure; // �R�[���o�b�N�֐��̎w��
	w.lpszClassName = L"DX12Sample"; // �A�v���P�[�V�����N���X��(�K���ł悢)
	w.hInstance = GetModuleHandle(nullptr); // �n���h���̎擾
	RegisterClassEx(&w); // �A�v���P�[�V�����N���X(�E�B���h�E�N���X�̎w���OS�ɓ`����)

	constexpr LONG window_width = 1920;
	constexpr LONG window_height = 1080;
	RECT wrc = { 0,0,window_width, window_height }; // �E�B���h�E�T�C�Y�����߂�
	AdjustWindowRect(&wrc, WS_OVERLAPPEDWINDOW, false); // �֐����g���ăE�B���h�E�̃T�C�Y��␳����

	// �E�B���h�E�I�u�W�F�N�g�̐���
	HWND hwnd = CreateWindow
	(
		w.lpszClassName, // �N���X���w��
		L"DX12�e�X�g", // �^�C�g���o�[�̕���
		WS_OVERLAPPEDWINDOW, // �^�C�g���o�[�Ƌ��E��������E�B���h�E
		CW_USEDEFAULT, // �\��x���W�͂n�r�ɂ��C��
		CW_USEDEFAULT, // �\��y���W�͂n�r�ɂ��C��
		wrc.right - wrc.left, // �E�B���h�E��
		wrc.bottom - wrc.top, // �E�B���h�E��
		nullptr, // �e�E�B���h�E�n���h��
		nullptr, // ���j���[�n���h��
		w.hInstance, // �Ăяo���A�v���P�[�V�����n���h��
		nullptr // �ǉ��p�����[�^
	);

	// �E�B���h�E�\��
	ShowWindow(hwnd, SW_SHOW);

#ifdef _DEBUG
	EnableDebugLayer();
#endif // _DEBUG


	IDXGIFactory6* _dxgiFactory = nullptr;
#ifdef _DEBUG
	auto result = CreateDXGIFactory2( DXGI_CREATE_FACTORY_DEBUG, IID_PPV_ARGS(&_dxgiFactory));
#else
	auto result = CreateDXGIFactory1(IID_PPV_ARGS(&_dxgiFactory));
#endif // _DEBUG

	// �A�_�v�^�[�̗񋓗p
	std::vector<IDXGIAdapter*> adapters;

	// �����ɓ���̖��O�����A�_�v�^�[�I�u�W�F�N�g������
	IDXGIAdapter* tmpAdapter = nullptr;
	for (int i = 0; _dxgiFactory->EnumAdapters(i, &tmpAdapter) != DXGI_ERROR_NOT_FOUND; ++i)
	{
		adapters.push_back(tmpAdapter);
	}

	for (auto adpt : adapters)
	{
		DXGI_ADAPTER_DESC adesc = {};
		adpt->GetDesc(&adesc); // �A�_�v�^�[�̐����I�u�W�F�N�g�̎擾
		std::wstring strDesc = adesc.Description;

		// �T�������A�_�v�^�[�̖��O���m�F
		if (strDesc.find(L"NVIDIA") != std::string::npos)
		{
			tmpAdapter = adpt;
			break;
		}
	}


	auto levels = std::to_array
	({
			D3D_FEATURE_LEVEL_12_1,
			D3D_FEATURE_LEVEL_12_0,
			D3D_FEATURE_LEVEL_11_1,
			D3D_FEATURE_LEVEL_11_0,
		});

	// Durect3D�f�o�C�X�̏�����
	D3D_FEATURE_LEVEL featureLevel;
	ID3D12Device* _dev = nullptr;

	for (auto lv : levels)
	{
		if (D3D12CreateDevice(tmpAdapter, lv, IID_PPV_ARGS(&_dev)) == S_OK)
		{
			featureLevel = lv;
			break; // �����\�ȃo�[�W���������������烋�[�v��ł��؂�
		}
	}

	ID3D12CommandAllocator* _cmdAllocator = nullptr;
	ID3D12GraphicsCommandList* _cmdList = nullptr;
	result = _dev->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&_cmdAllocator));
	result = _dev->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, _cmdAllocator, nullptr, IID_PPV_ARGS(&_cmdList));


	ID3D12CommandQueue* _cmdQueue = nullptr;
	D3D12_COMMAND_QUEUE_DESC cmdQueueDesc = {};

	// �^�C���A�E�g�Ȃ�
	cmdQueueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;

	// �A�_�v�^�[��1�����g��Ȃ��Ƃ���0�ł悢
	cmdQueueDesc.NodeMask = 0;

	// �v���C�I���e�B�͓��Ɏw��Ȃ�
	cmdQueueDesc.Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL;

	// �R�}���h���X�g�ƍ��킹��
	cmdQueueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;

	// �L���[����
	result = _dev->CreateCommandQueue(&cmdQueueDesc, IID_PPV_ARGS(&_cmdQueue));


	DXGI_SWAP_CHAIN_DESC1 swapchainDesc = {};
	swapchainDesc.Width = window_width;
	swapchainDesc.Height = window_height;
	swapchainDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	swapchainDesc.Stereo = false;
	swapchainDesc.SampleDesc.Count = 1;
	swapchainDesc.SampleDesc.Quality = 0;
	swapchainDesc.BufferUsage = DXGI_USAGE_BACK_BUFFER;
	swapchainDesc.BufferCount = 2;

	// �o�b�N�o�b�t�@�[�͐L�яk�݉\
	swapchainDesc.Scaling = DXGI_SCALING_STRETCH;

	// �t���b�v��͑��₩�ɔj��
	swapchainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;

	// ���Ɏw��Ȃ�
	swapchainDesc.AlphaMode = DXGI_ALPHA_MODE_UNSPECIFIED;

	// �E�B���h�E�̃t���X�N���[���؂�ւ��\
	swapchainDesc.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;

	IDXGISwapChain4* _swapchain = nullptr;

	result = _dxgiFactory->CreateSwapChainForHwnd
	(
		_cmdQueue,
		hwnd,
		&swapchainDesc,
		nullptr,
		nullptr,
		(IDXGISwapChain1**)&_swapchain
	);

	D3D12_DESCRIPTOR_HEAP_DESC heapDesc{};
	heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV; // �����_�[�^�[�Q�b�g�r���[�Ȃ̂�RTV
	heapDesc.NodeMask = 0;
	heapDesc.NumDescriptors = 2; // �\����2��
	heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE; // ���Ɏw��Ȃ�

	ID3D12DescriptorHeap* rtvHeaps = nullptr;
	result = _dev->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(&rtvHeaps));

	DXGI_SWAP_CHAIN_DESC swcDesc = {};
	result = _swapchain->GetDesc(&swcDesc);

	// SRGB�����_�[�^�[�Q�b�g�r���[�ݒ�
	D3D12_RENDER_TARGET_VIEW_DESC rtvDesc;

	rtvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB; // �K���}�␳����(sRGB)
	rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;

	std::vector<ID3D12Resource*> _backBuffers(swcDesc.BufferCount);
	for (UINT idx = 0; idx < swcDesc.BufferCount; ++idx)
	{
		result = _swapchain->GetBuffer(idx, IID_PPV_ARGS(&_backBuffers[idx]));
		D3D12_CPU_DESCRIPTOR_HANDLE handle = rtvHeaps->GetCPUDescriptorHandleForHeapStart();
		handle.ptr += idx * _dev->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

		_dev->CreateRenderTargetView
		(
			_backBuffers[idx],
			nullptr,
			handle
		);
	}

	auto bbIdx = _swapchain->GetCurrentBackBufferIndex();
	auto rtvH = rtvHeaps->GetCPUDescriptorHandleForHeapStart();
	rtvH.ptr += bbIdx * _dev->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

	D3D12_RESOURCE_BARRIER BarrierDesc = {};
	BarrierDesc.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION; // �J��
	BarrierDesc.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE; // ���Ɏw��Ȃ�
	BarrierDesc.Transition.pResource = _backBuffers[bbIdx]; // �o�b�N�o�b�t�@���\�[�X
	BarrierDesc.Transition.Subresource = 0;
	BarrierDesc.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT; // ���O��PRESENT���
	BarrierDesc.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET; // �����烌���_�[�^�[�Q�b�g���
	_cmdList->ResourceBarrier(1, &BarrierDesc); //�@�o���A�w����s


	_cmdList->OMSetRenderTargets(1, &rtvH, true, nullptr);

	// ��ʂ̃N���A
	std::array<float, 4>clearColor = { 0.3f,0.3f,0.3f,1.0f };
	_cmdList->ClearRenderTargetView(rtvH, clearColor.data(), 0, nullptr);

	// WIC�e�N�X�`���̃��[�h
	DirectX::TexMetadata metadata = {};
	DirectX::ScratchImage scratchImg = {};

	result = DirectX::LoadFromWICFile
	(
		L"img/textest.png",
		DirectX::WIC_FLAGS_NONE,
		&metadata,
		scratchImg
	);

	auto img = scratchImg.GetImage(0, 0, 0); // ���f�[�^���o

	struct TexRGBA
	{
		unsigned char R, G, B, A;
	};

	std::vector<TexRGBA> texturedata(256 * 256);
	for (auto& rgba : texturedata)
	{
		rgba.R = rand() % 256;
		rgba.G = rand() % 256;
		rgba.B = rand() % 256;
		rgba.A = 255; // ����1.0�Ƃ���
	}

	// WriteToSubresource�œ]�����邽�߂̃q�[�v�ݒ�
	D3D12_HEAP_PROPERTIES heapprop = {};

	// ����Ȑݒ�Ȃ̂�DEFAULT�ł�UPLOAD�ł��Ȃ�
	heapprop.Type = D3D12_HEAP_TYPE_CUSTOM;

	// ���C�g�o�b�N
	heapprop.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_WRITE_BACK;

	// �]����L0�A�܂�CPU�����璼�ڍs��
	heapprop.MemoryPoolPreference = D3D12_MEMORY_POOL_L0;

	// �P��A�_�v�^�̂���0
	heapprop.CreationNodeMask = 0;
	heapprop.VisibleNodeMask = 0;

	D3D12_RESOURCE_DESC resdesc = {};
	resdesc.Format = metadata.format;
	resdesc.Width = metadata.width; // ��
	resdesc.Height = metadata.height; // ����
	resdesc.DepthOrArraySize = static_cast<UINT16>(metadata.arraySize);
	resdesc.SampleDesc.Count = 1; // �ʏ�e�N�X�`���Ȃ̂ŃA�����G���A�V���O���Ȃ�
	resdesc.SampleDesc.Quality = 0; // �N�I���e�B�͍Œ�
	resdesc.MipLevels = static_cast<UINT16>(metadata.mipLevels);
	resdesc.Dimension = static_cast<D3D12_RESOURCE_DIMENSION>(metadata.dimension);
	resdesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN; // ���C�A�E�g�͌��肵�Ȃ�
	resdesc.Flags = D3D12_RESOURCE_FLAG_NONE; // ���Ƀt���O�Ȃ�

	ID3D12Resource* texbuff = nullptr;
	result = _dev->CreateCommittedResource
	(
		&heapprop,
		D3D12_HEAP_FLAG_NONE, // ���Ɏw��Ȃ�
		&resdesc,
		D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, // �e�N�X�`���p�w��
		nullptr,
		IID_PPV_ARGS(&texbuff)
	);

	result = texbuff->WriteToSubresource
	(
		0,
		nullptr, // �S�̈�փR�s�[
		img->pixels, // ���f�[�^�A�h���X
		img->rowPitch, // 1���C���T�C�Y
		img->slicePitch // 1���T�C�Y
	);

	ID3D12DescriptorHeap* texDescHeap = nullptr;
	D3D12_DESCRIPTOR_HEAP_DESC descHeapDesc = {};

	// �V�F�[�_�[���猩����悤��
	descHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;

	// �}�X�N��0
	descHeapDesc.NodeMask = 0;

	// �r���[�͍��̂Ƃ���1����
	descHeapDesc.NumDescriptors = 1;

	// �V�F�[�_�[���\�[�X�r���[�p
	descHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;

	// ����
	result = _dev->CreateDescriptorHeap(&descHeapDesc, IID_PPV_ARGS(&texDescHeap));

	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Format = metadata.format;
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING; // ��q
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING; // ��q
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D; // 2D�e�N�X�`��
	srvDesc.Texture2D.MipLevels = 1; // �~�b�v�}�b�v�͎g�p���Ȃ��̂�1

	_dev->CreateShaderResourceView
	(
		texbuff, // �r���[�Ɗ֘A�t����o�b�t�@�[
		&srvDesc, // �Ȃقǐݒ肵���e�N�X�`���ݒ���
		texDescHeap->GetCPUDescriptorHandleForHeapStart() // �q�[�v�̂ǂ��Ɋ��蓖�Ă邩
	);


	struct Vertex
	{
		DirectX::XMFLOAT3 pos; // xyz���W
		DirectX::XMFLOAT2 uv; // uv���W
	};

	Vertex vertices[] =
	{
		{{-0.4f,-0.7f,0.0f},{0.0f,1.0f} }, // ����
		{{-0.4f,0.7f,0.0f},{0.0f,0.0f} }, // ����
		{{0.4f,-0.7f,0.0f},{1.0f,1.0f} }, // �E��
		{{0.4f,0.7f,0.0f},{1.0f,0.0f} }, // �E��
	};

	unsigned short indices[] =
	{
		0, 1, 2,
		2, 1, 3
	};

	heapprop = {};
	heapprop.Type = D3D12_HEAP_TYPE_UPLOAD;
	heapprop.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
	heapprop.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;

	resdesc = {};
	resdesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	resdesc.Width = sizeof(vertices); // ���_��񂪓��邾���̃T�C�Y
	resdesc.Height = 1;
	resdesc.DepthOrArraySize = 1;
	resdesc.MipLevels = 1;
	resdesc.Format = DXGI_FORMAT_UNKNOWN;
	resdesc.SampleDesc.Count = 1;
	resdesc.Flags = D3D12_RESOURCE_FLAG_NONE;
	resdesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

	ID3D12Resource* vertBuff = nullptr;

	result = _dev->CreateCommittedResource
	(
		&heapprop,
		D3D12_HEAP_FLAG_NONE,
		&resdesc,
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(&vertBuff)
	);

	Vertex* vertMap = nullptr;
	result = vertBuff->Map(0, nullptr, (void**)&vertMap);
	std::copy(std::begin(vertices), std::end(vertices), vertMap);
	vertBuff->Unmap(0, nullptr);

	D3D12_VERTEX_BUFFER_VIEW vbView = {};
	vbView.BufferLocation = vertBuff->GetGPUVirtualAddress(); // �o�b�t�@�[�̉��z�A�h���X
	vbView.SizeInBytes = sizeof(vertices); // �S�o�C�g��
	vbView.StrideInBytes = sizeof(vertices[0]); // 1���_������̃o�C�g��

	ID3D12Resource* idxBuff = nullptr;

	// �ݒ�́A�o�b�t�@�[�̃T�C�Y�ȊO�A���_�o�b�t�@�̐ݒ���g���܂킵�Ă悢
	resdesc.Width = sizeof(indices);

	result = _dev->CreateCommittedResource
	(
		&heapprop,
		D3D12_HEAP_FLAG_NONE,
		&resdesc,
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(&idxBuff)
	);

	// ������o�b�t�@�[�ɃC���f�b�N�X�f�[�^���R�s�[
	unsigned short* mappedIdx = nullptr;
	result = idxBuff->Map(0, nullptr, (void**)&mappedIdx);
	std::copy(std::begin(indices), std::end(indices), mappedIdx);
	idxBuff->Unmap(0, nullptr);

	// �C���f�b�N�X�o�b�t�@�r���[���쐬
	D3D12_INDEX_BUFFER_VIEW ibView = {};
	ibView.BufferLocation = idxBuff->GetGPUVirtualAddress(); // �o�b�t�@�[�̉��z�A�h���X
	ibView.Format = DXGI_FORMAT_R16_UINT;
	ibView.SizeInBytes = sizeof(indices); // �S�o�C�g��


	ID3DBlob* _vsBlob = nullptr;
	ID3DBlob* _psBlob = nullptr;
	ID3DBlob* errorBlob = nullptr;


	result = D3DCompileFromFile
	(
		L"BasicVertexShader.hlsl", // �V�F�[�_�[��
		nullptr, // define�͂Ȃ�
		D3D_COMPILE_STANDARD_FILE_INCLUDE, // �C���N���[�h�̓f�t�H���g
		"BasicVS", "vs_5_0", // �֐���BasicVS�A�ΏۃV�F�[�_�[��vs_5_0
		D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION, // �f�o�b�O�p����эœK���͂Ȃ�
		0,
		&_vsBlob, &errorBlob // �G���[����errorBlob�Ƀ��b�Z�[�W������
	);

	if (FAILED(result))
	{
		if (result == HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND))
		{
			::OutputDebugStringA("�t�@�C������������܂���");
			return 0;
		}
		else
		{
			std::string errstr; // �󂯎��p
			errstr.resize(errorBlob->GetBufferSize()); // �K�v�ȃT�C�Y���m��

			// �f�[�^�R�s�[
			std::copy_n((char*)errorBlob->GetBufferPointer(), errorBlob->GetBufferSize(), errstr.begin());
			errstr += "\n";

			::OutputDebugStringA(errstr.c_str()); // �f�[�^�\��
		}
	}



	result = D3DCompileFromFile
	(
		L"BasicPixelShader.hlsl", // �V�F�[�_�[��
		nullptr, // define�͂Ȃ�
		D3D_COMPILE_STANDARD_FILE_INCLUDE, // �C���N���[�h�̓f�t�H���g
		"BasicPS", "ps_5_0", // �֐���BasicPS�A�ΏۃV�F�[�_�[��ps_5_0
		D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION, // �f�o�b�O�p����эœK���͂Ȃ�
		0,
		&_psBlob, &errorBlob // �G���[����errorBlob�Ƀ��b�Z�[�W������
	);

	if (FAILED(result))
	{
		if (result == HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND))
		{
			::OutputDebugStringA("�t�@�C������������܂���");
			return 0;
		}
		else
		{
			std::string errstr; // �󂯎��p
			errstr.resize(errorBlob->GetBufferSize()); // �K�v�ȃT�C�Y���m��

			// �f�[�^�R�s�[
			std::copy_n((char*)errorBlob->GetBufferPointer(), errorBlob->GetBufferSize(), errstr.begin());
			errstr += "\n";

			::OutputDebugStringA(errstr.c_str()); // �f�[�^�\��
		}
	}

	D3D12_INPUT_ELEMENT_DESC inputlayout[] =
	{
		{ // ���W���
			"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0,
			D3D12_APPEND_ALIGNED_ELEMENT,
			D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0
		},
		{ // uv
			"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0,
			D3D12_APPEND_ALIGNED_ELEMENT,
			D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0
		},
	};

	D3D12_STATIC_SAMPLER_DESC samplerDesc = {};
	samplerDesc.AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP; // �������̌J��Ԃ�
	samplerDesc.AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP; // �c�����̌J��Ԃ�
	samplerDesc.AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP; // ���s���̌J��Ԃ�
	samplerDesc.BorderColor = D3D12_STATIC_BORDER_COLOR_TRANSPARENT_BLACK; // �{�[�_�[�͍�
	samplerDesc.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR; // ���`���
	samplerDesc.MaxLOD = D3D12_FLOAT32_MAX; // �~�b�v�}�b�v�ő�l
	samplerDesc.MinLOD = 0.0f;// �~�b�v�}�b�v�ŏ��l
	samplerDesc.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL; // �s�N�Z���V�F�[�_�[���猩����
	samplerDesc.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER; // ���T���v�����O���Ȃ�

	D3D12_DESCRIPTOR_RANGE descTblRange = {};
	descTblRange.NumDescriptors = 1; // �e�N�X�`����1��
	descTblRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV; // ��ʂ̓e�N�X�`��
	descTblRange.BaseShaderRegister = 0; // 0�ԃX���b�g����
	descTblRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

	D3D12_ROOT_PARAMETER rootparam = {};
	rootparam.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;

	// �s�N�Z���V�F�[�_�[���猩����
	rootparam.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

	// �f�B�X�N���v�^�����W�̃A�h���X
	rootparam.DescriptorTable.pDescriptorRanges = &descTblRange;

	// �f�B�X�N���v�^�����W��
	rootparam.DescriptorTable.NumDescriptorRanges = 1;

	D3D12_ROOT_SIGNATURE_DESC rootSignatureDesc = {};
	rootSignatureDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
	rootSignatureDesc.pParameters = &rootparam; // ���[�g�p�����[�^�[�̐擪�A�h���X
	rootSignatureDesc.NumParameters = 1; // ���[�g�p�����[�^�[��
	rootSignatureDesc.pStaticSamplers = &samplerDesc;
	rootSignatureDesc.NumStaticSamplers = 1;

	ID3DBlob* rootSigBlob = nullptr;
	
	result = D3D12SerializeRootSignature
	(
		&rootSignatureDesc, // ���[�g�V�O�l�`���ݒ�
		D3D_ROOT_SIGNATURE_VERSION_1_0, // ���[�g�V�O�l�`���o�[�W����
		&rootSigBlob, // �V�F�[�_�[��������Ƃ��Ɠ���
		&errorBlob // �G���[����������
	);

	ID3D12RootSignature* rootsignature = nullptr;

	result = _dev->CreateRootSignature
	(
		0, // nodemask�B0�ł悢
		rootSigBlob->GetBufferPointer(), // �V�F�[�_�[�̂Ƃ��Ɠ��l
		rootSigBlob->GetBufferSize(), // �V�F�[�_�[�̂Ƃ��Ɠ��l
		IID_PPV_ARGS(&rootsignature)
	);

	rootSigBlob->Release(); // �s�v�ɂȂ����̂ŉ��U

	D3D12_GRAPHICS_PIPELINE_STATE_DESC gpipeline = {};
	gpipeline.pRootSignature = rootsignature;
	gpipeline.VS.pShaderBytecode = _vsBlob->GetBufferPointer();
	gpipeline.VS.BytecodeLength = _vsBlob->GetBufferSize();
	gpipeline.PS.pShaderBytecode = _psBlob->GetBufferPointer();
	gpipeline.PS.BytecodeLength = _psBlob->GetBufferSize();

	// �f�t�H���g�̃T���v���}�X�N��\���萔(0xffffffff)
	gpipeline.SampleMask = D3D12_DEFAULT_SAMPLE_MASK;

	// �܂��A���`�G�C���A�X�͎g��Ȃ�����false
	gpipeline.RasterizerState.MultisampleEnable = false;

	gpipeline.RasterizerState.CullMode = D3D12_CULL_MODE_NONE; // �J�����O���Ȃ�
	gpipeline.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID; // ���g��h��Ԃ�
	gpipeline.RasterizerState.DepthClipEnable = true; // �[�x�����̃N���b�s���O�͗L����

	gpipeline.BlendState.AlphaToCoverageEnable = false;
	gpipeline.BlendState.IndependentBlendEnable = false;

	D3D12_RENDER_TARGET_BLEND_DESC renderTargetBlendDesc = {};
	renderTargetBlendDesc.BlendEnable = false;
	renderTargetBlendDesc.LogicOpEnable = false;
	renderTargetBlendDesc.RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;

	gpipeline.BlendState.RenderTarget[0] = renderTargetBlendDesc;

	gpipeline.InputLayout.pInputElementDescs = inputlayout; // ���C�A�E�g�擪�A�h���X
	gpipeline.InputLayout.NumElements = _countof(inputlayout); // ���C�A�E�g�z��̗v�f��

	gpipeline.IBStripCutValue = D3D12_INDEX_BUFFER_STRIP_CUT_VALUE_DISABLED; // �J�b�g�Ȃ�

	// �O�p�`�ō\��
	gpipeline.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;

	gpipeline.NumRenderTargets = 1; // ����1�̂�

	gpipeline.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM; // 0�`1�ɐ��K�����ꂽRGBA

	gpipeline.SampleDesc.Count = 1; // �T���v�����O��1�s�N�Z���ɂ�
	gpipeline.SampleDesc.Quality = 0; // �N�I���e�B�͍Œ�

	ID3D12PipelineState* _pipelinestate = nullptr;

	result = _dev->CreateGraphicsPipelineState(&gpipeline, IID_PPV_ARGS(&_pipelinestate));

	D3D12_VIEWPORT viewport = {};
	viewport.Width = window_width; // �o�͐�̕�(�s�N�Z����)
	viewport.Height = window_height; // �o�͐�̍���(�s�N�Z����)
	viewport.TopLeftX = 0; // �o�͐�̍�����WX
	viewport.TopLeftY = 0; // �o�͐�̍�����WY
	viewport.MaxDepth = 1.0f; // �[�x�ő�l
	viewport.MinDepth = 0.0f; // �[�x�ŏ��l

	D3D12_RECT scissorrect = {};
	scissorrect.top = 0; // �؂蔲������W
	scissorrect.left = 0; // �؂蔲�������W
	scissorrect.right = scissorrect.left + window_width; // �؂蔲���E���W
	scissorrect.bottom = scissorrect.top + window_height; // �؂蔲�������W

	_cmdList->SetPipelineState(_pipelinestate);
	_cmdList->SetGraphicsRootSignature(rootsignature);
	_cmdList->SetDescriptorHeaps(1, &texDescHeap);

	_cmdList->SetGraphicsRootDescriptorTable
	(
		0, // ���[�g�p�����[�^�[�C���f�b�N�X
		texDescHeap->GetGPUDescriptorHandleForHeapStart() // �q�[�v�A�h���X
	);;

	_cmdList->RSSetViewports(1, &viewport);
	_cmdList->RSSetScissorRects(1, &scissorrect);
	_cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	_cmdList->IASetVertexBuffers(0, 1, &vbView);
	_cmdList->IASetIndexBuffer(&ibView);
	_cmdList->DrawIndexedInstanced(6, 1, 0, 0, 0);

	BarrierDesc.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
	BarrierDesc.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;
	_cmdList->ResourceBarrier(1, &BarrierDesc);

	// ���߂̃N���[�Y
	_cmdList->Close();

	// �R�}���h���X�g�̎��s
	ID3D12CommandList* cmdLists[] = { _cmdList };
	_cmdQueue->ExecuteCommandLists(1, cmdLists);

	ID3D12Fence* _fence = nullptr;
	UINT64 _fenceVal = 0;
	result = _dev->CreateFence(_fenceVal, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&_fence));

	_cmdQueue->Signal(_fence, ++_fenceVal);

	if (_fence->GetCompletedValue() != _fenceVal)
	{
		// �C�x���g�n���h���̎擾
		auto event = CreateEvent(nullptr, false, false, nullptr);

		_fence->SetEventOnCompletion(_fenceVal, event);

		// �C�x���g����������܂ő҂�������(INFINITE)
		WaitForSingleObject(event, INFINITE);

		// �C�x���g�n���h�������
		CloseHandle(event);
	}

	_cmdAllocator->Reset(); // �L���[���N���A
	_cmdList->Reset(_cmdAllocator, nullptr); // �ĂуR�}���h���X�g�����߂鏀��
		
	// �t���b�v
	_swapchain->Present(1, 0);



	MSG msg = {};

	while (true)
	{
		if (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE))
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}

		// �A�v���P�[�V�������I���Ƃ���message��WM_QUIT�ɂȂ�
		if (msg.message == WM_QUIT)
		{
			break;
		}
	}

	// �����N���X�͎g��Ȃ��̂œo�^��������
	UnregisterClass(w.lpszClassName, w.hInstance);	

	return 0;
}