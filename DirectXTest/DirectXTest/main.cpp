#include <Windows.h>
#include <tchar.h>
#include <d3d12.h>
#include <dxgi1_6.h>
#include <array>
#include <vector>
#include <DirectXMath.h>
#include <d3dcompiler.h>
#include <DirectXTex.h>
#include <d3dx12.h>
#ifdef _DEBUG
#include <iostream>
#endif

#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3dcompiler.lib")
#pragma comment(lib, "DirectXTex.lib")

using namespace std;

// @brief コンソール画面にフォーマット付き文字列を表示
// @param format フォーマット(%dとか%fとかの)
// @param 可変長引数
// @remarks この関数はデバッグ用です。デバッグ時にしか動作しません
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
	// ウィンドウが破棄されたら呼ばれる
	if (msg == WM_DESTROY)
	{
		PostQuitMessage(0); //OSに対して「もうこのアプリは終わる」と伝える
		return 0;
	}
	return DefWindowProc(hwnd, msg, wparam, lparam); // 既定の処理を粉う
}

void EnableDebugLayer()
{
	ID3D12Debug* debugLayer = nullptr;
	auto result = D3D12GetDebugInterface(IID_PPV_ARGS(&debugLayer));
	debugLayer->EnableDebugLayer(); // デバッグレイヤーを有効かする
	debugLayer->Release(); // 有効化したらインターフェイスを解放する
}

// アライメントにそろえたサイズを返す
// @param size 元のサイズ
// @param alignment アライメントサイズ
// @return アライメントをそろえたサイズ
size_t AlignmentedSize(size_t size, size_t alignment)
{
	return size + alignment - size % alignment;
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
	w.lpfnWndProc = (WNDPROC)WindowProcedure; // コールバック関数の指定
	w.lpszClassName = L"DX12Sample"; // アプリケーションクラス名(適当でよい)
	w.hInstance = GetModuleHandle(nullptr); // ハンドルの取得
	RegisterClassEx(&w); // アプリケーションクラス(ウィンドウクラスの指定をOSに伝える)

	constexpr LONG window_width = 1920;
	constexpr LONG window_height = 1080;
	RECT wrc = { 0,0,window_width, window_height }; // ウィンドウサイズを決める
	AdjustWindowRect(&wrc, WS_OVERLAPPEDWINDOW, false); // 関数を使ってウィンドウのサイズを補正する

	// ウィンドウオブジェクトの生成
	HWND hwnd = CreateWindow
	(
		w.lpszClassName, // クラス名指定
		L"DX12テスト", // タイトルバーの文字
		WS_OVERLAPPEDWINDOW, // タイトルバーと境界線があるウィンドウ
		CW_USEDEFAULT, // 表示x座標はＯＳにお任せ
		CW_USEDEFAULT, // 表示y座標はＯＳにお任せ
		wrc.right - wrc.left, // ウィンドウ幅
		wrc.bottom - wrc.top, // ウィンドウ高
		nullptr, // 親ウィンドウハンドル
		nullptr, // メニューハンドル
		w.hInstance, // 呼び出しアプリケーションハンドル
		nullptr // 追加パラメータ
	);

	// ウィンドウ表示
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

	// アダプターの列挙用
	std::vector<IDXGIAdapter*> adapters;

	// ここに特定の名前を持つアダプターオブジェクトが入る
	IDXGIAdapter* tmpAdapter = nullptr;
	for (int i = 0; _dxgiFactory->EnumAdapters(i, &tmpAdapter) != DXGI_ERROR_NOT_FOUND; ++i)
	{
		adapters.push_back(tmpAdapter);
	}

	for (auto adpt : adapters)
	{
		DXGI_ADAPTER_DESC adesc = {};
		adpt->GetDesc(&adesc); // アダプターの説明オブジェクトの取得
		std::wstring strDesc = adesc.Description;

		// 探したいアダプターの名前を確認
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

	// Durect3Dデバイスの初期化
	D3D_FEATURE_LEVEL featureLevel;
	ID3D12Device* _dev = nullptr;

	for (auto lv : levels)
	{
		if (D3D12CreateDevice(tmpAdapter, lv, IID_PPV_ARGS(&_dev)) == S_OK)
		{
			featureLevel = lv;
			break; // 生成可能なバージョンが見つかったらループを打ち切り
		}
	}

	ID3D12CommandAllocator* _cmdAllocator = nullptr;
	ID3D12GraphicsCommandList* _cmdList = nullptr;
	result = _dev->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&_cmdAllocator));
	result = _dev->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, _cmdAllocator, nullptr, IID_PPV_ARGS(&_cmdList));


	ID3D12CommandQueue* _cmdQueue = nullptr;
	D3D12_COMMAND_QUEUE_DESC cmdQueueDesc = {};

	// タイムアウトなし
	cmdQueueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;

	// アダプターを1つしか使わないときは0でよい
	cmdQueueDesc.NodeMask = 0;

	// プライオリティは特に指定なし
	cmdQueueDesc.Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL;

	// コマンドリストと合わせる
	cmdQueueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;

	// キュー生成
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

	// バックバッファーは伸び縮み可能
	swapchainDesc.Scaling = DXGI_SCALING_STRETCH;

	// フリップ後は速やかに破棄
	swapchainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;

	// 特に指定なし
	swapchainDesc.AlphaMode = DXGI_ALPHA_MODE_UNSPECIFIED;

	// ウィンドウ⇔フルスクリーン切り替え可能
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
	heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV; // レンダーターゲットビューなのでRTV
	heapDesc.NodeMask = 0;
	heapDesc.NumDescriptors = 2; // 表裏の2つ
	heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE; // 特に指定なし

	ID3D12DescriptorHeap* rtvHeaps = nullptr;
	result = _dev->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(&rtvHeaps));

	DXGI_SWAP_CHAIN_DESC swcDesc = {};
	result = _swapchain->GetDesc(&swcDesc);

	// SRGBレンダーターゲットビュー設定
	D3D12_RENDER_TARGET_VIEW_DESC rtvDesc;

	rtvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB; // ガンマ補正あり(sRGB)
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


	// WICテクスチャのロード
	DirectX::TexMetadata metadata = {};
	DirectX::ScratchImage scratchImg = {};

	result = DirectX::LoadFromWICFile
	(
		L"img/textest.png",
		DirectX::WIC_FLAGS_NONE,
		&metadata,
		scratchImg
	);

	auto img = scratchImg.GetImage(0, 0, 0); // 生データ抽出

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
		rgba.A = 255; // αは1.0とする
	}

	// WriteToSubresourceで転送するためのヒープ設定
	D3D12_HEAP_PROPERTIES heapprop = {};

	//// 特殊な設定なのでDEFAULTでもUPLOADでもない
	//heapprop.Type = D3D12_HEAP_TYPE_CUSTOM;

	//// ライトバック
	//heapprop.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_WRITE_BACK;

	//// 転送はL0、つまりCPU側から直接行う
	//heapprop.MemoryPoolPreference = D3D12_MEMORY_POOL_L0;

	//// 単一アダプタのため0
	//heapprop.CreationNodeMask = 0;
	//heapprop.VisibleNodeMask = 0;

	D3D12_RESOURCE_DESC resdesc = {};
	//resdesc.Format = metadata.format;
	//resdesc.Width = metadata.width; // 幅
	//resdesc.Height = metadata.height; // 高さ
	//resdesc.DepthOrArraySize = static_cast<UINT16>(metadata.arraySize);
	//resdesc.SampleDesc.Count = 1; // 通常テクスチャなのでアンリエリアシングしない
	//resdesc.SampleDesc.Quality = 0; // クオリティは最低
	//resdesc.MipLevels = static_cast<UINT16>(metadata.mipLevels);
	//resdesc.Dimension = static_cast<D3D12_RESOURCE_DIMENSION>(metadata.dimension);
	//resdesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN; // レイアウトは決定しない
	//resdesc.Flags = D3D12_RESOURCE_FLAG_NONE; // 特にフラグなし

	ID3D12Resource* texbuff = nullptr;
	//result = _dev->CreateCommittedResource
	//(
	//	&heapprop,
	//	D3D12_HEAP_FLAG_NONE, // 特に指定なし
	//	&resdesc,
	//	D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, // テクスチャ用指定
	//	nullptr,
	//	IID_PPV_ARGS(&texbuff)
	//);

	//result = texbuff->WriteToSubresource
	//(
	//	0,
	//	nullptr, // 全領域へコピー
	//	img->pixels, // 元データアドレス
	//	img->rowPitch, // 1ラインサイズ
	//	img->slicePitch // 1枚サイズ
	//);

	// 中間バッファーとしてのアップロードヒープ設定
	D3D12_HEAP_PROPERTIES uploadHeapProp = {};

	// マップ可能にするため、UPLOADにする
	uploadHeapProp.Type = D3D12_HEAP_TYPE_UPLOAD;

	// アップロード用に使用すること前提なのでUNKNOWNでよい
	uploadHeapProp.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
	uploadHeapProp.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
	uploadHeapProp.CreationNodeMask = 0; // 単一アダプターのため0
	uploadHeapProp.VisibleNodeMask = 0; // 単一アダプターのため0

	resdesc.Format = DXGI_FORMAT_UNKNOWN; // 単なるデータの塊なのでUNKNOWN
	resdesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER; // 単なるバッファーとして指定

	resdesc.Width = AlignmentedSize(img->rowPitch, D3D12_TEXTURE_DATA_PITCH_ALIGNMENT) * img->height; // データサイズ
	resdesc.Height = 1;
	resdesc.DepthOrArraySize = 1;
	resdesc.MipLevels = 1;

	resdesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR; // 連続したデータ
	resdesc.Flags = D3D12_RESOURCE_FLAG_NONE; // 特にフラグなし

	resdesc.SampleDesc.Count = 1; // 通常テクスチャなのでアンチエイリアシングしない
	resdesc.SampleDesc.Quality = 0;

	// 中間バッファー作成
	ID3D12Resource* uploadbuff = nullptr;

	result = _dev->CreateCommittedResource
	(
		&uploadHeapProp,
		D3D12_HEAP_FLAG_NONE, // 特に指定なし
		&resdesc,
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(&uploadbuff)
	);

	// テクスチャのためのヒープ設定
	D3D12_HEAP_PROPERTIES texHeapProp = {};

	texHeapProp.Type = D3D12_HEAP_TYPE_DEFAULT; // テクスチャ用
	texHeapProp.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
	texHeapProp.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
	texHeapProp.CreationNodeMask = 0; // 単一アダプターのため0
	texHeapProp.VisibleNodeMask = 0; // 単一アダプターのため0

	// リソース設定(変数は使いまわし)
	resdesc.Format = metadata.format;
	resdesc.Width = metadata.width; // 幅
	resdesc.Height = metadata.height; // 高さ
	resdesc.DepthOrArraySize = static_cast<UINT16>(metadata.arraySize); // 2Dで配列でもないので1
	resdesc.MipLevels = static_cast<UINT16>(metadata.mipLevels); // ミップマップしないのでミップ数は1つ
	resdesc.Dimension = static_cast<D3D12_RESOURCE_DIMENSION>(metadata.dimension);
	resdesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;

	// テクスチャバッファーの作成
	texbuff = nullptr;
	result = _dev->CreateCommittedResource
	(
		&texHeapProp,
		D3D12_HEAP_FLAG_NONE, // 特に指定なし
		&resdesc,
		D3D12_RESOURCE_STATE_COPY_DEST, // コピー先
		nullptr,
		IID_PPV_ARGS(&texbuff)
	);

	uint8_t* mapforImg = nullptr; // image->pixelsと同じ型にする
	result = uploadbuff->Map(0, nullptr, (void**)&mapforImg); // マップ

	auto srcAddress = img->pixels;
	auto rowPitch = AlignmentedSize(img->rowPitch, D3D12_TEXTURE_DATA_PITCH_ALIGNMENT);

	for (size_t y = 0; y < img->height; ++y)
	{
		std::copy_n(srcAddress, rowPitch, mapforImg); // コピー

		// 1行ごとのつじつまを合わせる
		srcAddress += img->rowPitch;
		mapforImg += rowPitch;
	}

	//std::copy_n(img->pixels, img->slicePitch, mapforImg); // コピー
	uploadbuff->Unmap(0, nullptr); // アンマップ

	D3D12_TEXTURE_COPY_LOCATION src = {};
	
	// コピー元(アップロード側)設定
	src.pResource = uploadbuff; // 中間バッファー
	src.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT; // フットプリント指定
	src.PlacedFootprint.Offset = 0;
	src.PlacedFootprint.Footprint.Width = metadata.width;
	src.PlacedFootprint.Footprint.Height = metadata.height;
	src.PlacedFootprint.Footprint.Depth = metadata.depth;
	src.PlacedFootprint.Footprint.RowPitch = AlignmentedSize(img->rowPitch, D3D12_TEXTURE_DATA_PITCH_ALIGNMENT);
	src.PlacedFootprint.Footprint.Format = img->format;


	D3D12_TEXTURE_COPY_LOCATION dst = {};

	// コピー先設定
	dst.pResource = texbuff;
	dst.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
	dst.SubresourceIndex = 0;

	_cmdList->CopyTextureRegion(&dst, 0, 0, 0, &src, nullptr);

	BarrierDesc.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	BarrierDesc.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
	BarrierDesc.Transition.pResource = texbuff;
	BarrierDesc.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
	BarrierDesc.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST; // ここが重要
	BarrierDesc.Transition.StateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE; // ここも重要

	_cmdList->ResourceBarrier(1, &BarrierDesc);
	_cmdList->Close();

	ID3D12CommandList* cmdLists[] = { _cmdList };
	_cmdQueue->ExecuteCommandLists(1, cmdLists);

	ID3D12Fence* _fence = nullptr;
	UINT64 _fenceVal = 0;
	result = _dev->CreateFence(_fenceVal, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&_fence));

	_cmdQueue->Signal(_fence, ++_fenceVal);

	if (_fence->GetCompletedValue() != _fenceVal)
	{
		// イベントハンドルの取得
		auto event = CreateEvent(nullptr, false, false, nullptr);

		_fence->SetEventOnCompletion(_fenceVal, event);

		// イベントが発生するまで待ち続ける(INFINITE)
		WaitForSingleObject(event, INFINITE);

		// イベントハンドルを閉じる
		CloseHandle(event);
	}

	_cmdAllocator->Reset(); // キューをクリア
	_cmdList->Reset(_cmdAllocator, nullptr); // 再びコマンドリストをためる準備

	ID3D12DescriptorHeap* basicDescHeap = nullptr;
	D3D12_DESCRIPTOR_HEAP_DESC descHeapDesc = {};

	// シェーダーから見えるように
	descHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;

	// マスクは0
	descHeapDesc.NodeMask = 0;

	// SRV1つとCBV1つ
	descHeapDesc.NumDescriptors = 2;

	// シェーダーリソースビュー用
	descHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;

	// 生成
	result = _dev->CreateDescriptorHeap(&descHeapDesc, IID_PPV_ARGS(&basicDescHeap));

	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Format = metadata.format;
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING; // 後述
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING; // 後述
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D; // 2Dテクスチャ
	srvDesc.Texture2D.MipLevels = 1; // ミップマップは使用しないので1

	// ディスクリプタの先頭ハンドルを取得しておく
	auto basicHeapHandle = basicDescHeap->GetCPUDescriptorHandleForHeapStart();

	// シェーダーリソースビューの作成
	_dev->CreateShaderResourceView
	(
		texbuff, // ビューと関連付けるバッファー
		&srvDesc, // 席ほど設定したテクスチャ設定情報
		basicHeapHandle // 先頭の場所を示すハンドル
	);



	DirectX::XMMATRIX matrix = DirectX::XMMatrixRotationY(DirectX::XM_PIDIV4);

	DirectX::XMFLOAT3 eye(0, 0, -5);
	DirectX::XMFLOAT3 target(0, 0, 0);
	DirectX::XMFLOAT3 up(0, 1, 0);

	matrix *= DirectX::XMMatrixLookAtLH(DirectX::XMLoadFloat3(&eye), DirectX::XMLoadFloat3(&target), DirectX::XMLoadFloat3(&up));
	matrix *= DirectX::XMMatrixPerspectiveFovLH
	(
		DirectX::XM_PIDIV2, // 画角は90°
		static_cast<float>(window_width) / static_cast<float>(window_height), // アスペクト比
		1.0f, // 近いほう
		10.0f // 遠いほう
	);


	//matrix.r[0].m128_f32[0] = 2.0f / window_width;
	//matrix.r[1].m128_f32[1] = -2.0f / window_height;

	//matrix.r[3].m128_f32[0] = -1.0f;
	//matrix.r[3].m128_f32[1] = 1.0f;



	// 定数バッファー作成
	ID3D12Resource* constBuff = nullptr;

	heapprop = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
	resdesc = CD3DX12_RESOURCE_DESC::Buffer((sizeof(matrix) + 0xff) & ~0xff);

	_dev->CreateCommittedResource
	(
		&heapprop,
		D3D12_HEAP_FLAG_NONE,
		&resdesc,
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(&constBuff)
	);

	DirectX::XMMATRIX* mapMatrix; // マップ先を示すポインター
	result = constBuff->Map(0, nullptr, (void**)&mapMatrix); // マップ
	*mapMatrix = matrix; // 行列の内容をコピー

	// 次の場所に移動
	basicHeapHandle.ptr += _dev->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

	D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc = {};
	cbvDesc.BufferLocation = constBuff->GetGPUVirtualAddress();
	cbvDesc.SizeInBytes = static_cast<UINT>(constBuff->GetDesc().Width);

	// 定数バッファビューの作成
	_dev->CreateConstantBufferView(&cbvDesc, basicHeapHandle);


	struct Vertex
	{
		DirectX::XMFLOAT3 pos; // xyz座標
		DirectX::XMFLOAT2 uv; // uv座標
	};

	Vertex vertices[] =
	{
		{{-1.0f,-1.0f,0.0f},{0.0f,1.0f} }, // 左下
		{{-1.0f,1.0f,0.0f},{0.0f,0.0f} }, // 左上
		{{1.0f,-1.0f,0.0f},{1.0f,1.0f} }, // 右下
		{{1.0f,1.0f,0.0f},{1.0f,0.0f} }, // 右上
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
	resdesc.Width = sizeof(vertices); // 頂点情報が入るだけのサイズ
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
	vbView.BufferLocation = vertBuff->GetGPUVirtualAddress(); // バッファーの仮想アドレス
	vbView.SizeInBytes = sizeof(vertices); // 全バイト数
	vbView.StrideInBytes = sizeof(vertices[0]); // 1頂点あたりのバイト数

	ID3D12Resource* idxBuff = nullptr;

	// 設定は、バッファーのサイズ以外、頂点バッファの設定を使いまわしてよい
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

	// 作ったバッファーにインデックスデータをコピー
	unsigned short* mappedIdx = nullptr;
	result = idxBuff->Map(0, nullptr, (void**)&mappedIdx);
	std::copy(std::begin(indices), std::end(indices), mappedIdx);
	idxBuff->Unmap(0, nullptr);

	// インデックスバッファビューを作成
	D3D12_INDEX_BUFFER_VIEW ibView = {};
	ibView.BufferLocation = idxBuff->GetGPUVirtualAddress(); // バッファーの仮想アドレス
	ibView.Format = DXGI_FORMAT_R16_UINT;
	ibView.SizeInBytes = sizeof(indices); // 全バイト数


	ID3DBlob* _vsBlob = nullptr;
	ID3DBlob* _psBlob = nullptr;
	ID3DBlob* errorBlob = nullptr;


	result = D3DCompileFromFile
	(
		L"BasicVertexShader.hlsl", // シェーダー名
		nullptr, // defineはなし
		D3D_COMPILE_STANDARD_FILE_INCLUDE, // インクルードはデフォルト
		"BasicVS", "vs_5_0", // 関数はBasicVS、対象シェーダーはvs_5_0
		D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION, // デバッグ用および最適化はなし
		0,
		&_vsBlob, &errorBlob // エラー時はerrorBlobにメッセージが入る
	);

	if (FAILED(result))
	{
		if (result == HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND))
		{
			::OutputDebugStringA("ファイルが見当たりません");
			return 0;
		}
		else
		{
			std::string errstr; // 受け取り用
			errstr.resize(errorBlob->GetBufferSize()); // 必要なサイズを確保

			// データコピー
			std::copy_n((char*)errorBlob->GetBufferPointer(), errorBlob->GetBufferSize(), errstr.begin());
			errstr += "\n";

			::OutputDebugStringA(errstr.c_str()); // データ表示
		}
	}



	result = D3DCompileFromFile
	(
		L"BasicPixelShader.hlsl", // シェーダー名
		nullptr, // defineはなし
		D3D_COMPILE_STANDARD_FILE_INCLUDE, // インクルードはデフォルト
		"BasicPS", "ps_5_0", // 関数はBasicPS、対象シェーダーはps_5_0
		D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION, // デバッグ用および最適化はなし
		0,
		&_psBlob, &errorBlob // エラー時はerrorBlobにメッセージが入る
	);

	if (FAILED(result))
	{
		if (result == HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND))
		{
			::OutputDebugStringA("ファイルが見当たりません");
			return 0;
		}
		else
		{
			std::string errstr; // 受け取り用
			errstr.resize(errorBlob->GetBufferSize()); // 必要なサイズを確保

			// データコピー
			std::copy_n((char*)errorBlob->GetBufferPointer(), errorBlob->GetBufferSize(), errstr.begin());
			errstr += "\n";

			::OutputDebugStringA(errstr.c_str()); // データ表示
		}
	}

	D3D12_INPUT_ELEMENT_DESC inputlayout[] =
	{
		{ // 座標情報
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
	samplerDesc.AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP; // 横方向の繰り返し
	samplerDesc.AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP; // 縦方向の繰り返し
	samplerDesc.AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP; // 奥行きの繰り返し
	samplerDesc.BorderColor = D3D12_STATIC_BORDER_COLOR_TRANSPARENT_BLACK; // ボーダーは黒
	samplerDesc.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR; // 線形補間
	samplerDesc.MaxLOD = D3D12_FLOAT32_MAX; // ミップマップ最大値
	samplerDesc.MinLOD = 0.0f;// ミップマップ最小値
	samplerDesc.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL; // ピクセルシェーダーから見える
	samplerDesc.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER; // リサンプリングしない

	D3D12_DESCRIPTOR_RANGE descTblRange[2] = {};
	descTblRange[0].NumDescriptors = 1; // テクスチャは1つ
	descTblRange[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV; // 種別はテクスチャ
	descTblRange[0].BaseShaderRegister = 0; // 0番スロットから
	descTblRange[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND; // 定数用レジスター0番

	descTblRange[1].NumDescriptors = 1; // 定数1つ
	descTblRange[1].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_CBV; // 種別は定数
	descTblRange[1].BaseShaderRegister = 0; // 0番スロットから
	descTblRange[1].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;


	D3D12_ROOT_PARAMETER rootparam = {};
	rootparam.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;

	// 配列先頭アドレス
	rootparam.DescriptorTable.pDescriptorRanges = descTblRange;

	// ディスクリプタレンジ数
	rootparam.DescriptorTable.NumDescriptorRanges = 2;

	// すべてのシェーダーから見える
	rootparam.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

	D3D12_ROOT_SIGNATURE_DESC rootSignatureDesc = {};
	rootSignatureDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
	rootSignatureDesc.pParameters = &rootparam; // ルートパラメーターの先頭アドレス
	rootSignatureDesc.NumParameters = 1; // ルートパラメーター数
	rootSignatureDesc.pStaticSamplers = &samplerDesc;
	rootSignatureDesc.NumStaticSamplers = 1;

	ID3DBlob* rootSigBlob = nullptr;
	
	result = D3D12SerializeRootSignature
	(
		&rootSignatureDesc, // ルートシグネチャ設定
		D3D_ROOT_SIGNATURE_VERSION_1_0, // ルートシグネチャバージョン
		&rootSigBlob, // シェーダーを作ったときと同じ
		&errorBlob // エラー処理も同じ
	);

	ID3D12RootSignature* rootsignature = nullptr;

	result = _dev->CreateRootSignature
	(
		0, // nodemask。0でよい
		rootSigBlob->GetBufferPointer(), // シェーダーのときと同様
		rootSigBlob->GetBufferSize(), // シェーダーのときと同様
		IID_PPV_ARGS(&rootsignature)
	);

	rootSigBlob->Release(); // 不要になったので解散

	D3D12_GRAPHICS_PIPELINE_STATE_DESC gpipeline = {};
	gpipeline.pRootSignature = rootsignature;
	gpipeline.VS.pShaderBytecode = _vsBlob->GetBufferPointer();
	gpipeline.VS.BytecodeLength = _vsBlob->GetBufferSize();
	gpipeline.PS.pShaderBytecode = _psBlob->GetBufferPointer();
	gpipeline.PS.BytecodeLength = _psBlob->GetBufferSize();

	// デフォルトのサンプルマスクを表す定数(0xffffffff)
	gpipeline.SampleMask = D3D12_DEFAULT_SAMPLE_MASK;

	// まだアンチエイリアスは使わないためfalse
	gpipeline.RasterizerState.MultisampleEnable = false;

	gpipeline.RasterizerState.CullMode = D3D12_CULL_MODE_NONE; // カリングしない
	gpipeline.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID; // 中身を塗りつぶす
	gpipeline.RasterizerState.DepthClipEnable = true; // 深度方向のクリッピングは有効に

	gpipeline.BlendState.AlphaToCoverageEnable = false;
	gpipeline.BlendState.IndependentBlendEnable = false;

	D3D12_RENDER_TARGET_BLEND_DESC renderTargetBlendDesc = {};
	renderTargetBlendDesc.BlendEnable = false;
	renderTargetBlendDesc.LogicOpEnable = false;
	renderTargetBlendDesc.RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;

	gpipeline.BlendState.RenderTarget[0] = renderTargetBlendDesc;

	gpipeline.InputLayout.pInputElementDescs = inputlayout; // レイアウト先頭アドレス
	gpipeline.InputLayout.NumElements = _countof(inputlayout); // レイアウト配列の要素数

	gpipeline.IBStripCutValue = D3D12_INDEX_BUFFER_STRIP_CUT_VALUE_DISABLED; // カットなし

	// 三角形で構成
	gpipeline.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;

	gpipeline.NumRenderTargets = 1; // 今は1つのみ

	gpipeline.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM; // 0～1に正規化されたRGBA

	gpipeline.SampleDesc.Count = 1; // サンプリングは1ピクセルにつき
	gpipeline.SampleDesc.Quality = 0; // クオリティは最低

	ID3D12PipelineState* _pipelinestate = nullptr;

	result = _dev->CreateGraphicsPipelineState(&gpipeline, IID_PPV_ARGS(&_pipelinestate));

	D3D12_VIEWPORT viewport = {};
	viewport.Width = window_width; // 出力先の幅(ピクセル数)
	viewport.Height = window_height; // 出力先の高さ(ピクセル数)
	viewport.TopLeftX = 0; // 出力先の左上座標X
	viewport.TopLeftY = 0; // 出力先の左上座標Y
	viewport.MaxDepth = 1.0f; // 深度最大値
	viewport.MinDepth = 0.0f; // 深度最小値

	D3D12_RECT scissorrect = {};
	scissorrect.top = 0; // 切り抜き上座標
	scissorrect.left = 0; // 切り抜き左座標
	scissorrect.right = scissorrect.left + window_width; // 切り抜き右座標
	scissorrect.bottom = scissorrect.top + window_height; // 切り抜き下座標


	BarrierDesc.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION; // 遷移
	BarrierDesc.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE; // 特に指定なし
	BarrierDesc.Transition.pResource = _backBuffers[bbIdx]; // バックバッファリソース
	BarrierDesc.Transition.Subresource = 0;
	BarrierDesc.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT; // 直前はPRESENT状態
	BarrierDesc.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET; // 今からレンダーターゲット状態
	_cmdList->ResourceBarrier(1, &BarrierDesc); //　バリア指定実行


	_cmdList->OMSetRenderTargets(1, &rtvH, true, nullptr);

	// 画面のクリア
	std::array<float, 4>clearColor = { 0.3f,0.3f,0.3f,1.0f };
	_cmdList->ClearRenderTargetView(rtvH, clearColor.data(), 0, nullptr);

	_cmdList->SetPipelineState(_pipelinestate);
	_cmdList->SetGraphicsRootSignature(rootsignature);
	_cmdList->SetDescriptorHeaps(1, &basicDescHeap);

	auto headpHandle = basicDescHeap->GetGPUDescriptorHandleForHeapStart();

	_cmdList->SetGraphicsRootDescriptorTable
	(
		0, // ルートパラメーターインデックス
		headpHandle // ヒープアドレス
	);

	_cmdList->RSSetViewports(1, &viewport);
	_cmdList->RSSetScissorRects(1, &scissorrect);
	_cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	_cmdList->IASetVertexBuffers(0, 1, &vbView);
	_cmdList->IASetIndexBuffer(&ibView);
	_cmdList->DrawIndexedInstanced(6, 1, 0, 0, 0);

	BarrierDesc.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION; // 遷移
	BarrierDesc.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE; // 特に指定なし
	BarrierDesc.Transition.pResource = _backBuffers[bbIdx]; // バックバッファリソース
	BarrierDesc.Transition.Subresource = 0;
	BarrierDesc.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
	BarrierDesc.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;
	_cmdList->ResourceBarrier(1, &BarrierDesc);

	// 命令のクローズ
	_cmdList->Close();

	// コマンドリストの実行
	//ID3D12CommandList* cmdLists[] = { _cmdList };
	_cmdQueue->ExecuteCommandLists(1, cmdLists);

	_cmdQueue->Signal(_fence, ++_fenceVal);

	if (_fence->GetCompletedValue() != _fenceVal)
	{
		// イベントハンドルの取得
		auto event = CreateEvent(nullptr, false, false, nullptr);

		_fence->SetEventOnCompletion(_fenceVal, event);

		// イベントが発生するまで待ち続ける(INFINITE)
		WaitForSingleObject(event, INFINITE);

		// イベントハンドルを閉じる
		CloseHandle(event);
	}

	_cmdAllocator->Reset(); // キューをクリア
	_cmdList->Reset(_cmdAllocator, nullptr); // 再びコマンドリストをためる準備
		
	// フリップ
	_swapchain->Present(1, 0);



	MSG msg = {};

	while (true)
	{
		if (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE))
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}

		// アプリケーションが終わるときにmessageがWM_QUITになる
		if (msg.message == WM_QUIT)
		{
			break;
		}
	}

	// もうクラスは使わないので登録解除する
	UnregisterClass(w.lpszClassName, w.hInstance);	

	return 0;
}