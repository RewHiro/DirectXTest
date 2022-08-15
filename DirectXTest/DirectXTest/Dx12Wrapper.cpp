#include "Dx12Wrapper.h"
#include <cassert>
#include <d3dx12.h>
#include "Application.h"

#pragma comment(lib,"DirectXTex.lib")
#pragma comment(lib,"d3d12.lib")
#pragma comment(lib,"dxgi.lib")
#pragma comment(lib,"d3dcompiler.lib")

using namespace Microsoft::WRL;
using namespace std;
using namespace DirectX;

namespace {
	// モデルのパスとテクスチャのパスから合成パスを得る
	// @param modelPath アプリケーションから見たpmdモデルのパス
	// @param texPath PMDモデルから見たテクスチャのパス
	// @param return アプリケーションから見たテクスチャのパス
	std::string GetTexturePathFromModelAndTexPath
	(
		const std::string& modelPath,
		const char* texPath
	)
	{
		// ファイルのフォルダ区切りは\と/の2種類が使用される可能性があり
		// ともかく末尾の\と/を得られればいいので、双方のrfindをとり比較する
		// int型に代入しているのは見つからなかった場合はrfindがepos(-1->0xffffffff)を返すため
		int pathIndex1 = modelPath.rfind('/');
		int pathIndex2 = modelPath.rfind('\\');
		auto pathIndex = max(pathIndex1, pathIndex2);
		auto folderPath = modelPath.substr(0, pathIndex + 1);
		return folderPath + texPath;
	}

	// ファイル名から拡張子を取得する
	// @param path 対象のパス文字列
	// @return 拡張子
	std::string GetExtension(const std::string& path)
	{
		int idx = path.rfind('.');
		return path.substr(idx + 1, path.length() - idx - 1);
	}

	// ファイル名から拡張子を取得する(ワイド文字版)
	// @param path 対象のパス文字列
	// @return 拡張子
	std::wstring GetExtension(const std::wstring& path)
	{
		int idx = path.rfind(L'.');
		return path.substr(idx + 1, path.length() - idx - 1);
	}

	// テクスチャのパスをセパレーター文字で分離する
	// @param path 対象のパス文字列
	// @param slitter 区切り文字
	// @return 分離前後の文字列ペア
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

	// std::string(マルチバイト文字列)からstd::wstring(ワイド文字列)を得る
	// @param str マルチバイト文字列
	// @param return 変換されたワイド文字列
	std::wstring GetWideStringFromString(const std::string& str)
	{
		// 呼び出し1回目(文字列数を得る)
		auto num1 = MultiByteToWideChar
		(
			CP_ACP,
			MB_PRECOMPOSED | MB_ERR_INVALID_CHARS,
			str.c_str(),
			-1,
			nullptr,
			0
		);

		std::wstring wstr; // stringのwchar_t版
		wstr.resize(num1); // 得られた文字列数でリサイズ

		// 呼び出し2回目(確保済みのwstrに変換文字列をコピー)
		auto num2 = MultiByteToWideChar
		(
			CP_ACP,
			MB_PRECOMPOSED | MB_ERR_INVALID_CHARS,
			str.c_str(),
			-1,
			&wstr[0],
			num1
		);

		assert(num1 == num2); // 一応チェック
		return wstr;
	}

	void EnableDebugLayer()
	{
		ComPtr<ID3D12Debug> debugLayer = nullptr;
		auto result = D3D12GetDebugInterface(IID_PPV_ARGS(&debugLayer));
		debugLayer->EnableDebugLayer(); // デバッグレイヤーを有効かする
	}
}

HRESULT Dx12Wrapper::CreateFinalRenderTargets()
{
	DXGI_SWAP_CHAIN_DESC1 desc = {};
	auto result = _swapchain->GetDesc1(&desc);

	D3D12_DESCRIPTOR_HEAP_DESC heapDesc{};
	heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV; // レンダーターゲットビューなのでRTV
	heapDesc.NodeMask = 0;
	heapDesc.NumDescriptors = 2; // 表裏の2つ
	heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE; // 特に指定なし

	result = _dev->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(_rtvHeaps.ReleaseAndGetAddressOf()));

	if (FAILED(result)) {
		SUCCEEDED(result);
		return result;
	}

	DXGI_SWAP_CHAIN_DESC swcDesc = {};
	result = _swapchain->GetDesc(&swcDesc);
	_backBuffers.resize(swcDesc.BufferCount);

	D3D12_CPU_DESCRIPTOR_HANDLE handle = _rtvHeaps->GetCPUDescriptorHandleForHeapStart();


	// SRGBレンダーターゲットビュー設定
	D3D12_RENDER_TARGET_VIEW_DESC rtvDesc = {};
	rtvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB; // ガンマ補正あり(sRGB)
	rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;

	for (UINT i = 0; i < swcDesc.BufferCount; ++i)
	{
		result = _swapchain->GetBuffer(i, IID_PPV_ARGS(&_backBuffers[i]));
		assert(SUCCEEDED(result));
		rtvDesc.Format = _backBuffers[i]->GetDesc().Format;
		_dev->CreateRenderTargetView
		(
			_backBuffers[i],
			&rtvDesc,
			handle
		);

		handle.ptr += _dev->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
	}

	_viewport.reset(new CD3DX12_VIEWPORT(_backBuffers[0]));
	_scissorrect.reset(new CD3DX12_RECT(0, 0, desc.Width, desc.Height));

	return result;
}

HRESULT Dx12Wrapper::CreateDepthStencilView()
{
	DXGI_SWAP_CHAIN_DESC1 desc = {};
	auto result = _swapchain->GetDesc1(&desc);

	// 深度バッファーの作成
	D3D12_RESOURCE_DESC resdesc = {};
	resdesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D; // 2次元のテクスチャデータ
	resdesc.DepthOrArraySize = 1; // テクスチャ配列でも、3Dテクスチャでもない
	resdesc.Width = desc.Width; // 幅と高さはレンダーターゲットと同じ
	resdesc.Height = desc.Height; // 同上
	resdesc.Format = DXGI_FORMAT_D32_FLOAT; // 深度値書き込む用フォーマット
	resdesc.SampleDesc.Count = 1; // サンプルは1ピクセルあたり1つ
	resdesc.SampleDesc.Quality = 0;
	resdesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL; // デプスステンシルとして使用
	resdesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
	resdesc.MipLevels = 1;
	resdesc.Alignment = 0;

	// 深度値用ヒーププロパティ
	auto depthHeapProp = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);

	// このクリアバリューが重要な意味を持つ
	CD3DX12_CLEAR_VALUE depthClearValue(DXGI_FORMAT_D32_FLOAT, 1.0f, 0);

	result = _dev->CreateCommittedResource
	(
		&depthHeapProp,
		D3D12_HEAP_FLAG_NONE,
		&resdesc,
		D3D12_RESOURCE_STATE_DEPTH_WRITE, // 深度値書き込みに使用
		&depthClearValue,
		IID_PPV_ARGS(_depthBuffer.ReleaseAndGetAddressOf())
	);

	if (FAILED(result)) {
		// エラー処理
		return result;
	}

	D3D12_DESCRIPTOR_HEAP_DESC dsvHeapDesc = {}; // 深度に使うことがわかればよい
	dsvHeapDesc.NumDescriptors = 1; // 深度ビューは1つ
	dsvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV; // デプスステンシルビューとして使う
	dsvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;

	result = _dev->CreateDescriptorHeap(&dsvHeapDesc, IID_PPV_ARGS(_dsvHeaps.ReleaseAndGetAddressOf()));

	// 深度ビュー作成
	D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc = {};
	dsvDesc.Format = DXGI_FORMAT_D32_FLOAT; // 深度値に32ビット使用
	dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D; // 2Dテクスチャ
	dsvDesc.Flags = D3D12_DSV_FLAG_NONE; // フラグは特になし

	_dev->CreateDepthStencilView
	(
		_depthBuffer.Get(),
		&dsvDesc,
		_dsvHeaps->GetCPUDescriptorHandleForHeapStart()
	);

	return result;
}

HRESULT Dx12Wrapper::CreateSwapChain(const HWND& hwnd)
{
	RECT rc = {};
	::GetWindowRect(hwnd, &rc);

	DXGI_SWAP_CHAIN_DESC1 swapchainDesc = {};
	swapchainDesc.Width = _winSize.cx;
	swapchainDesc.Height = _winSize.cy;
	swapchainDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	swapchainDesc.Stereo = false;
	swapchainDesc.SampleDesc.Count = 1;
	swapchainDesc.SampleDesc.Quality = 0;
	swapchainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	swapchainDesc.BufferCount = 2;

	// バックバッファーは伸び縮み可能
	swapchainDesc.Scaling = DXGI_SCALING_STRETCH;

	// フリップ後は速やかに破棄
	swapchainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;

	// 特に指定なし
	swapchainDesc.AlphaMode = DXGI_ALPHA_MODE_UNSPECIFIED;

	// ウィンドウ⇔フルスクリーン切り替え可能
	swapchainDesc.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;

	auto result = _dxgiFactory->CreateSwapChainForHwnd
	(
		_cmdQueue.Get(),
		hwnd,
		&swapchainDesc,
		nullptr,
		nullptr,
		(IDXGISwapChain1**)_swapchain.ReleaseAndGetAddressOf()
	);

	return result;
}

HRESULT Dx12Wrapper::InitializeDXGIDevice()
{
	UINT flagsDXGI = 0;
	flagsDXGI |= DXGI_CREATE_FACTORY_DEBUG;

	auto result = CreateDXGIFactory2(flagsDXGI, IID_PPV_ARGS(_dxgiFactory.ReleaseAndGetAddressOf()));

	D3D_FEATURE_LEVEL levels[] = 
	{
			D3D_FEATURE_LEVEL_12_1,
			D3D_FEATURE_LEVEL_12_0,
			D3D_FEATURE_LEVEL_11_1,
			D3D_FEATURE_LEVEL_11_0,
	};

	if (FAILED(result)) {
		return result;
	}

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

	result = S_FALSE;

	// Durect3Dデバイスの初期化
	D3D_FEATURE_LEVEL featureLevel;

	for (auto lv : levels)
	{
		if (SUCCEEDED( D3D12CreateDevice(tmpAdapter, lv, IID_PPV_ARGS(_dev.ReleaseAndGetAddressOf()))))
		{
			featureLevel = lv;
			result = S_OK;
			break; // 生成可能なバージョンが見つかったらループを打ち切り
		}
	}

	return result;
}

HRESULT Dx12Wrapper::InitializeCommand()
{
	auto result = _dev->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(_cmdAllocator.ReleaseAndGetAddressOf()));

	if (FAILED(result)) {
		assert(0);
		return result;
	}

	result = _dev->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, _cmdAllocator.Get(), nullptr, IID_PPV_ARGS(_cmdList.ReleaseAndGetAddressOf()));

	if (FAILED(result)) {
		assert(0);
		return result;
	}

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
	result = _dev->CreateCommandQueue(&cmdQueueDesc, IID_PPV_ARGS(_cmdQueue.ReleaseAndGetAddressOf()));

	assert(SUCCEEDED(result));

	return result;
}

HRESULT Dx12Wrapper::CreateSceneView()
{
	DXGI_SWAP_CHAIN_DESC1 desc = {};
	auto result = _swapchain->GetDesc1(&desc);
	auto heapprop = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
	auto resdesc = CD3DX12_RESOURCE_DESC::Buffer((sizeof(SceneData) + 0xff) & ~0xff);

	// 定数バッファー作成
	result = _dev->CreateCommittedResource
	(
		&heapprop,
		D3D12_HEAP_FLAG_NONE,
		&resdesc,
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(_sceneConstBuff.ReleaseAndGetAddressOf())
	);

	if (FAILED(result)) {
		assert(SUCCEEDED(result));
		return result;
	}

	_mappedSceneData = nullptr; // マップ先を示すポインタ
	result = _sceneConstBuff->Map(0, nullptr, (void**)&_mappedSceneData); // マップ

	DirectX::XMFLOAT3 eye(0, 10, -15);
	DirectX::XMFLOAT3 target(0, 10, 0);
	DirectX::XMFLOAT3 up(0, 1, 0);

	_mappedSceneData->view = DirectX::XMMatrixLookAtLH(DirectX::XMLoadFloat3(&eye), DirectX::XMLoadFloat3(&target), DirectX::XMLoadFloat3(&up));
	_mappedSceneData->proj = DirectX::XMMatrixPerspectiveFovLH
	(
		DirectX::XM_PIDIV2, // 画角は90°
		static_cast<float>(desc.Width) / static_cast<float>(desc.Height), // アスペクト比
		0.1f, // 近いほう
		1000.0f // 遠いほう
	);
	_mappedSceneData->eye = eye;

	D3D12_DESCRIPTOR_HEAP_DESC descHeapDesc = {};
	descHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	descHeapDesc.NodeMask = 0;
	descHeapDesc.NumDescriptors = 1;
	descHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	result = _dev->CreateDescriptorHeap(&descHeapDesc, IID_PPV_ARGS(_sceneDescHeap.ReleaseAndGetAddressOf()));

	auto heapHandle = _sceneDescHeap->GetCPUDescriptorHandleForHeapStart();

	D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc = {};
	cbvDesc.BufferLocation = _sceneConstBuff->GetGPUVirtualAddress();
	cbvDesc.SizeInBytes = static_cast<UINT>(_sceneConstBuff->GetDesc().Width);

	// 定数バッファビューの作成
	_dev->CreateConstantBufferView(&cbvDesc, heapHandle);

	return result;
}

HRESULT Dx12Wrapper::CreatePeraResource()
{
	// 作成済みのヒープ情報を使ってもう1枚作る
	auto heapDesc = _rtvHeaps->GetDesc();

	// 使っているバックバッファーの情報を利用する
	auto& bbuff = _backBuffers[0];
	auto resDesc = bbuff->GetDesc();

	D3D12_HEAP_PROPERTIES heapProp = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);

	// レンダリング時のクリア値と同じ値
	float clsClr[4] = { 0.5f,0.5f,0.5f,1.0f };
	CD3DX12_CLEAR_VALUE clearValue = CD3DX12_CLEAR_VALUE(DXGI_FORMAT_R8G8B8A8_UNORM, clsClr);

	auto result = _dev->CreateCommittedResource
	(
		&heapProp
		, D3D12_HEAP_FLAG_NONE
		, &resDesc
		, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE
		, &clearValue
		, IID_PPV_ARGS(_peraResource.ReleaseAndGetAddressOf())
	);

	if (FAILED(result))
	{
		assert(SUCCEEDED(result));
		return result;
	}

	result = _dev->CreateCommittedResource
	(
		&heapProp
		, D3D12_HEAP_FLAG_NONE
		, &resDesc
		, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE
		, &clearValue
		, IID_PPV_ARGS(_peraResource2.ReleaseAndGetAddressOf())
	);

	if (FAILED(result))
	{
		assert(SUCCEEDED(result));
		return result;
	}

	// RTV用ヒープを作る
	heapDesc.NumDescriptors = 2;
	result = _dev->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(_peraRTVHeap.ReleaseAndGetAddressOf()));

	D3D12_RENDER_TARGET_VIEW_DESC rtvDesc = {};
	rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
	rtvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;

	auto rtvhandle = _peraRTVHeap->GetCPUDescriptorHandleForHeapStart();

	// レンダーターゲットビュー(RTV)を作る
	_dev->CreateRenderTargetView
	(
		_peraResource.Get()
		, &rtvDesc
		, rtvhandle
	);

	rtvhandle.ptr += _dev->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

	// レンダーターゲットビュー(RTV)を作る
	_dev->CreateRenderTargetView
	(
		_peraResource2.Get()
		, &rtvDesc
		, rtvhandle
	);

	// SRV用ヒープを作る
	heapDesc.NumDescriptors = 2;
	heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	result = _dev->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(_peraSRVHeap.ReleaseAndGetAddressOf()));

	D3D12_SHADER_RESOURCE_VIEW_DESC srcDesc = {};
	srcDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	srcDesc.Format = rtvDesc.Format;
	srcDesc.Texture2D.MipLevels = 1;
	srcDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;

	auto srvhandle = _peraSRVHeap->GetCPUDescriptorHandleForHeapStart();

	// シェーダーリソースビュー(SRV)を作る
	_dev->CreateShaderResourceView
	(
		_peraResource.Get()
		, &srcDesc
		, _peraSRVHeap->GetCPUDescriptorHandleForHeapStart()
	);

	srvhandle.ptr += _dev->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);


	// シェーダーリソースビュー(SRV)を作る
	_dev->CreateShaderResourceView
	(
		_peraResource2.Get()
		, &srcDesc
		, srvhandle
	);

	struct PeraVertex
	{
		XMFLOAT3 pos;
		XMFLOAT2 uv;
	};

	PeraVertex pv[4] =
	{
		{{ -1, -1, 0.1f},{0,1}} // 左下
		,{{ -1, 1, 0.1f},{0,0}} // 左上
		,{{ 1, -1, 0.1f},{1,1}} // 右下
		,{{ 1, 1, 0.1f},{1,0}} // 右上
	};

	heapProp = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
	auto resdesc = CD3DX12_RESOURCE_DESC::Buffer(sizeof(pv));

	result = _dev->CreateCommittedResource
	(
		&heapProp,D3D12_HEAP_FLAG_NONE
		,&resdesc
		,D3D12_RESOURCE_STATE_GENERIC_READ
		, nullptr
		, IID_PPV_ARGS(_peraVB.ReleaseAndGetAddressOf())
	);

	if (FAILED(result))
	{
		assert(SUCCEEDED(result));
		return result;
	}

	_peraVBV.BufferLocation = _peraVB->GetGPUVirtualAddress();
	_peraVBV.SizeInBytes = sizeof(pv);
	_peraVBV.StrideInBytes = sizeof(PeraVertex);

	PeraVertex* mappedPera = nullptr;
	_peraVB->Map(0, nullptr, (void**)&mappedPera);
	copy(begin(pv), end(pv), mappedPera);
	_peraVB->Unmap(0, nullptr);

	D3D12_INPUT_ELEMENT_DESC layout[2] =
	{
		{
			"POSITION"
			,0
			,DXGI_FORMAT_R32G32B32_FLOAT
			,0
			,D3D12_APPEND_ALIGNED_ELEMENT
			,D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA
			,0
		}
		,{
			"TEXCOORD"
			,0
			,DXGI_FORMAT_R32G32_FLOAT
			,0
			,D3D12_APPEND_ALIGNED_ELEMENT
			,D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA
			,0
		}
	};

	D3D12_GRAPHICS_PIPELINE_STATE_DESC gpsDesc = {};
	gpsDesc.InputLayout.NumElements = _countof(layout);
	gpsDesc.InputLayout.pInputElementDescs = layout;

	ComPtr<ID3DBlob> vs = nullptr;
	ComPtr<ID3DBlob> ps = nullptr;
	ComPtr<ID3DBlob> errorBlob = nullptr;

	result = D3DCompileFromFile
	(
		L"peraVertex.hlsl", // シェーダー名
		nullptr, // defineはなし
		D3D_COMPILE_STANDARD_FILE_INCLUDE, // インクルードはデフォルト
		"main", "vs_5_0", // 関数はmain、対象シェーダーはvs_5_0
		D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION, // デバッグ用および最適化はなし
		0,
		vs.ReleaseAndGetAddressOf(), errorBlob.ReleaseAndGetAddressOf()// エラー時はerrorBlobにメッセージが入る
	);

	if (!CheckShaderCompileResult(result, errorBlob.Get()))
	{
		assert(0);
		return result;
	}

	result = D3DCompileFromFile
	(
		L"peraPixel.hlsl", // シェーダー名
		nullptr, // defineはなし
		D3D_COMPILE_STANDARD_FILE_INCLUDE, // インクルードはデフォルト
		"main", "ps_5_0", // 関数はmain、対象シェーダーはps_5_0
		D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION, // デバッグ用および最適化はなし
		0,
		ps.ReleaseAndGetAddressOf(), errorBlob.ReleaseAndGetAddressOf() // エラー時はerrorBlobにメッセージが入る
	);

	if (!CheckShaderCompileResult(result, errorBlob.Get()))
	{
		assert(0);
		return result;
	}

	gpsDesc.VS = CD3DX12_SHADER_BYTECODE(vs.Get());
	gpsDesc.PS = CD3DX12_SHADER_BYTECODE(ps.Get());

	gpsDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);

	gpsDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;

	gpsDesc.NumRenderTargets = 1; // 今は1つのみ

	gpsDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM; // 0～1に正規化されたRGBA

	gpsDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);

	gpsDesc.SampleMask = D3D12_DEFAULT_SAMPLE_MASK;

	gpsDesc.SampleDesc.Count = 1; // サンプリングは1ピクセルにつき
	gpsDesc.SampleDesc.Quality = 0; // クオリティは最低

	gpsDesc.Flags = D3D12_PIPELINE_STATE_FLAG_NONE;

	gpsDesc.DepthStencilState.DepthEnable = false;
	gpsDesc.DepthStencilState.StencilEnable = false;

	D3D12_DESCRIPTOR_HEAP_DESC materialDescHeapDesc = {};
	materialDescHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	materialDescHeapDesc.NodeMask = 0;
	materialDescHeapDesc.NumDescriptors = 3;
	materialDescHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;

	result = _dev->CreateDescriptorHeap(&materialDescHeapDesc, IID_PPV_ARGS(_peraRegisterHeap.ReleaseAndGetAddressOf()));
	if (FAILED(result))
	{
		assert(SUCCEEDED(result));
		return result;
	}

	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Format = _peraResource->GetDesc().Format;
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING; // 後述
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D; // 2Dテクスチャ
	srvDesc.Texture2D.MipLevels = 1; // ミップマップは使用しないので1

	auto handle = _peraRegisterHeap->GetCPUDescriptorHandleForHeapStart();

	_dev->CreateShaderResourceView
	(
		_peraResource.Get(),
		&srvDesc,
		handle
	);

	handle.ptr += _dev->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

	_dev->CreateShaderResourceView
	(
		_peraResource2.Get(),
		&srvDesc,
		handle
	);

	handle.ptr += _dev->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

	D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc = {};
	cbvDesc.BufferLocation = _bokehParamBuffer->GetGPUVirtualAddress();
	cbvDesc.SizeInBytes = static_cast<UINT>(_bokehParamBuffer->GetDesc().Width);
	_dev->CreateConstantBufferView(&cbvDesc, handle);

	D3D12_DESCRIPTOR_RANGE range[3] = {};
	range[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV; // t
	range[0].BaseShaderRegister = 0; // 0
	range[0].NumDescriptors = 1;

	range[1].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_CBV; // t
	range[1].BaseShaderRegister = 0; // 0
	range[1].NumDescriptors = 1;

	range[2].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV; // t
	range[2].BaseShaderRegister = 1; // 1
	range[2].NumDescriptors = 1;

	D3D12_ROOT_PARAMETER rp[3] = {};
	rp[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	rp[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
	rp[0].DescriptorTable.pDescriptorRanges = &range[0];
	rp[0].DescriptorTable.NumDescriptorRanges = 1;

	rp[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	rp[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
	rp[1].DescriptorTable.pDescriptorRanges = &range[1];
	rp[1].DescriptorTable.NumDescriptorRanges = 1;

	rp[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	rp[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
	rp[2].DescriptorTable.pDescriptorRanges = &range[2];
	rp[2].DescriptorTable.NumDescriptorRanges = 1;

	D3D12_STATIC_SAMPLER_DESC sampler = CD3DX12_STATIC_SAMPLER_DESC(0); // s0

	CD3DX12_ROOT_SIGNATURE_DESC rsDesc = {};
	rsDesc.NumParameters = 3;
	rsDesc.pParameters = rp;
	rsDesc.NumStaticSamplers = 1;
	rsDesc.pStaticSamplers = &sampler;
	rsDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

	ComPtr<ID3DBlob> rsBlob = nullptr;

	result = D3D12SerializeRootSignature
	(
		&rsDesc, // ルートシグネチャ設定
		D3D_ROOT_SIGNATURE_VERSION_1, // ルートシグネチャバージョン
		rsBlob.ReleaseAndGetAddressOf(), // シェーダーを作ったときと同じ
		errorBlob.ReleaseAndGetAddressOf() // エラー処理も同じ
	);

	if (FAILED(result))
	{
		assert(SUCCEEDED(result));
		return result;
	}

	result = _dev->CreateRootSignature
	(
		0, // nodemask。0でよい
		rsBlob->GetBufferPointer(), // シェーダーのときと同様
		rsBlob->GetBufferSize(), // シェーダーのときと同様
		IID_PPV_ARGS(_rootSignature.ReleaseAndGetAddressOf())
	);

	if (FAILED(result))
	{
		assert(SUCCEEDED(result));
		return result;
	}



	gpsDesc.pRootSignature = _rootSignature.Get();

	result = _dev->CreateGraphicsPipelineState(&gpsDesc, IID_PPV_ARGS(_pipeline.ReleaseAndGetAddressOf()));

	if (FAILED(result))
	{
		assert(SUCCEEDED(result));
		return result;
	}


	result = D3DCompileFromFile
	(
		L"peraPixel.hlsl", // シェーダー名
		nullptr, // defineはなし
		D3D_COMPILE_STANDARD_FILE_INCLUDE, // インクルードはデフォルト
		"VerticalBokehPS", "ps_5_0", // 関数はmain、対象シェーダーはps_5_0
		D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION, // デバッグ用および最適化はなし
		0,
		ps.ReleaseAndGetAddressOf(), errorBlob.ReleaseAndGetAddressOf() // エラー時はerrorBlobにメッセージが入る
	);

	if (!CheckShaderCompileResult(result, errorBlob.Get()))
	{
		assert(0);
		return result;
	}

	gpsDesc.PS = CD3DX12_SHADER_BYTECODE(ps.Get());

	result = _dev->CreateGraphicsPipelineState(&gpsDesc, IID_PPV_ARGS(_pipeline2.ReleaseAndGetAddressOf()));

	if (FAILED(result))
	{
		assert(SUCCEEDED(result));
		return result;
	}

	return S_OK;
}

HRESULT Dx12Wrapper::CreateBokeResource()
{
	auto weights = GetGaussianWeights(8, 5.0f);
	D3D12_HEAP_PROPERTIES heapProp = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
	auto resdesc = CD3DX12_RESOURCE_DESC::Buffer((sizeof(weights[0]) * weights.size() + 0xff) & ~0xff);


	auto result = _dev->CreateCommittedResource
	(
		&heapProp
		,D3D12_HEAP_FLAG_NONE
		,&resdesc
		,D3D12_RESOURCE_STATE_GENERIC_READ
		,nullptr
		,IID_PPV_ARGS(_bokehParamBuffer.ReleaseAndGetAddressOf())
	);

	float* mappedWeight = nullptr;
	result = _bokehParamBuffer->Map(0, nullptr, (void**)&mappedWeight);
	copy(weights.begin(), weights.end(), mappedWeight);
	_bokehParamBuffer->Unmap(0, nullptr);

	return result;
}

void Dx12Wrapper::CreateTextureLoaderTable()
{

	_loadLambdaTable["sph"]
		= _loadLambdaTable["spa"]
		= _loadLambdaTable["bmp"]
		= _loadLambdaTable["png"]
		= _loadLambdaTable["jpg"]
		= [](const std::wstring& path, DirectX::TexMetadata* meta, DirectX::ScratchImage& img)->HRESULT
	{
		return DirectX::LoadFromWICFile(path.c_str(), DirectX::WIC_FLAGS_NONE, meta, img);
	};

	_loadLambdaTable["tga"]
		= [](const std::wstring& path, DirectX::TexMetadata* meta, DirectX::ScratchImage& img)->HRESULT
	{
		return DirectX::LoadFromTGAFile(path.c_str(), meta, img);
	};

	_loadLambdaTable["dds"]
		= [](const std::wstring& path, DirectX::TexMetadata* meta, DirectX::ScratchImage& img)->HRESULT
	{
		return DirectX::LoadFromDDSFile(path.c_str(), DirectX::DDS_FLAGS_NONE, meta, img);
	};
}

ID3D12Resource* Dx12Wrapper::CreateTextureFromFile(const char* texpath)
{
	string texPath = texpath;

	//WICテクスチャのロード
	DirectX::TexMetadata metadata = {};
	DirectX::ScratchImage scratchImg = {};

	auto wtexpath = GetWideStringFromString(texPath); // テクスチャのファイルパス

	auto ext = GetExtension(texPath); // 拡張子を取得

	auto result = _loadLambdaTable[ext](wtexpath, &metadata, scratchImg);

	if (FAILED(result))
	{
		return nullptr;
	}

	auto img = scratchImg.GetImage(0, 0, 0); // 生データ抽出

	// WriteToSubresourceで転送する用のヒープ設定
	D3D12_HEAP_PROPERTIES texHeapProp = CD3DX12_HEAP_PROPERTIES(D3D12_CPU_PAGE_PROPERTY_WRITE_BACK, D3D12_MEMORY_POOL_L0);

	D3D12_RESOURCE_DESC resDesc = CD3DX12_RESOURCE_DESC::Tex2D
	(
		metadata.format,
		metadata.width,
		metadata.height,
		static_cast<UINT16>(metadata.arraySize),
		static_cast<UINT16>(metadata.mipLevels)
	);
	resDesc.Dimension = static_cast<D3D12_RESOURCE_DIMENSION>(metadata.dimension);

	// バッファー作成
	ID3D12Resource* texbuff = nullptr;
	result = _dev->CreateCommittedResource
	(
		&texHeapProp,
		D3D12_HEAP_FLAG_NONE, // 特に指定なし
		&resDesc,
		D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
		nullptr,
		IID_PPV_ARGS(&texbuff)
	);

	if (FAILED(result))
	{
		return nullptr;
	}

	result = texbuff->WriteToSubresource
	(
		0,
		nullptr, // 全領域へコピー
		img->pixels, // 元データアドレス
		img->rowPitch, // 1ラインサイズ
		img->slicePitch // 全サイズ
	);

	if (FAILED(result))
	{
		return nullptr;
	}

	return texbuff;
}

bool Dx12Wrapper::CheckShaderCompileResult(HRESULT result, ID3DBlob* error)
{
	if (FAILED(result))
	{
		if (result == HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND))
		{
			::OutputDebugStringA("ファイルが見当たりません");
		}
		else
		{
			string errstr;
			errstr.resize(error->GetBufferSize());
			copy_n((char*)error->GetBufferPointer(), error->GetBufferSize(), errstr.begin());
			errstr += "\n";
			OutputDebugStringA(errstr.c_str());
		}
		return false;
	}
	else
	{
		return true;
	}
}

std::vector<float> Dx12Wrapper::GetGaussianWeights(size_t count, float s)
{
	std::vector<float> weights(count); // ウェイト配列返却用
	float x = 0.0f;
	float total = 0.0f;

	for (auto& wgt : weights)
	{
		wgt = expf(-(x * x) / (2 * s * s));
		total += wgt;
		x += 1.0f;
	}

	total = total * 2.0f - 1;

	for (auto& wgt : weights)
	{
		wgt /= total;
	}
	return weights;
}

bool Dx12Wrapper::CreateeffectBufferAndView()
{
	// 法線マップをロードする
	_effectTexBuffer = CreateTextureFromFile("normal/normalmap.jpg");

	// ポストエフェクト用ディスクリプタヒープ生成
	D3D12_DESCRIPTOR_HEAP_DESC heapDesc = {};
	heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	heapDesc.NumDescriptors = 1;
	heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;

	auto result = _dev->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(_effectSRVHeap.ReleaseAndGetAddressOf()));

	if (FAILED(result))
	{
		assert(SUCCEEDED(result));
		return false;
	}

	// ポストエフェクト用テクスチャビューを作る
	auto desc = _effectTexBuffer->GetDesc();

	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Format = desc.Format;
	srvDesc.Texture2D.MipLevels = 1;
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;

	_dev->CreateShaderResourceView(_effectTexBuffer.Get(), &srvDesc, _effectSRVHeap->GetCPUDescriptorHandleForHeapStart());

	return true;
}

Dx12Wrapper::Dx12Wrapper(HWND hwnd)
{
#ifdef _DEBUG
	// デバッグレイヤーをオンに
	EnableDebugLayer();
#endif // _DEBUG

	auto& app = Application::Instance();
	_winSize = app.GetWindowSize();

	// DirectX12関連初期化
	if (FAILED(InitializeDXGIDevice())) {
		assert(0);
		return;
	}
	if (FAILED(InitializeCommand())) {
		assert(0);
		return;
	}
	if (FAILED(CreateSwapChain(hwnd))) {
		assert(0);
		return;
	}
	if (FAILED(CreateFinalRenderTargets())) {
		assert(0);
		return;
	}

	if (FAILED(CreateSceneView())) {
		assert(0);
		return;
	}

	// テクスチャローダー関連初期化
	CreateTextureLoaderTable();

	// 深度バッファ作成
	if (FAILED(CreateDepthStencilView())) {
		assert(0);
		return;
	}

	// フェンスの作成
	if (FAILED(_dev->CreateFence(_fenceVal, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(_fence.ReleaseAndGetAddressOf())))) {
		assert(0);
		return;
	}

	if (!CreateeffectBufferAndView())
	{
		assert(0);
		return;
	}

	if (FAILED(CreateBokeResource())) {
		assert(0);
		return;
	}

	if (FAILED(CreatePeraResource())) {
		assert(0);
		return;
	}
	

}

Dx12Wrapper::~Dx12Wrapper()
{
}

void Dx12Wrapper::Update()
{
}

void Dx12Wrapper::PreDrawToPera1()
{
	auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(_peraResource.Get(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_RENDER_TARGET);
	_cmdList->ResourceBarrier(1, &barrier);

	auto rtvH = _peraRTVHeap->GetCPUDescriptorHandleForHeapStart();
	auto dsvH = _dsvHeaps->GetCPUDescriptorHandleForHeapStart();

	_cmdList->OMSetRenderTargets(1, &rtvH, false, &dsvH);

	// 画面のクリア
	float clearColor[] = { 0.5f,0.5f,0.5f,1.0f };
	_cmdList->ClearRenderTargetView(rtvH, clearColor, 0, nullptr);

	_cmdList->ClearDepthStencilView(dsvH, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);
}

void Dx12Wrapper::DrawToPera1()
{
	SetScene();

	auto wsize = Application::Instance().GetWindowSize();

	D3D12_VIEWPORT vp = CD3DX12_VIEWPORT(0.0f, 0.0f, static_cast<FLOAT>(wsize.cx), static_cast<FLOAT>(wsize.cy));
	_cmdList->RSSetViewports(1, &vp);

	CD3DX12_RECT rc(0, 0, wsize.cx, wsize.cy);
	_cmdList->RSSetScissorRects(1, &rc);
}

void Dx12Wrapper::PostDrawToPera1()
{
	auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(_peraResource.Get(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
	_cmdList->ResourceBarrier(1, &barrier);
}

void Dx12Wrapper::Clear()
{
	auto bbIdx = _swapchain->GetCurrentBackBufferIndex();
	auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(_backBuffers[bbIdx], D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET);
	_cmdList->ResourceBarrier(1, &barrier);

	auto rtvH = _rtvHeaps->GetCPUDescriptorHandleForHeapStart();
	rtvH.ptr += bbIdx * _dev->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

	_cmdList->OMSetRenderTargets(1, &rtvH, false, nullptr);

	// 画面のクリア
	float clearColor[] = { 0.2f,0.5f,0.5f,1.0f };
	_cmdList->ClearRenderTargetView(rtvH, clearColor, 0, nullptr);
}

void Dx12Wrapper::Draw()
{
	auto wsize = Application::Instance().GetWindowSize();

	D3D12_VIEWPORT vp = CD3DX12_VIEWPORT(0.0f, 0.0f, static_cast<FLOAT>(wsize.cx), static_cast<FLOAT>(wsize.cy));
	_cmdList->RSSetViewports(1, &vp);

	CD3DX12_RECT rc(0, 0, wsize.cx, wsize.cy);
	_cmdList->RSSetScissorRects(1, &rc);

	_cmdList->SetGraphicsRootSignature(_rootSignature.Get());
	//_cmdList->SetPipelineState(_pipeline2.Get());
	_cmdList->SetPipelineState(_pipeline.Get());


	auto handle = _peraRegisterHeap->GetGPUDescriptorHandleForHeapStart();

	// 
	//handle.ptr += _dev->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

	_cmdList->SetDescriptorHeaps(1, _peraRegisterHeap.GetAddressOf());
	_cmdList->SetGraphicsRootDescriptorTable(0, handle);

	handle.ptr += _dev->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	_cmdList->SetDescriptorHeaps(1, _peraRegisterHeap.GetAddressOf());
	_cmdList->SetGraphicsRootDescriptorTable(1, handle);

	_cmdList->SetDescriptorHeaps(1, _effectSRVHeap.GetAddressOf());
	_cmdList->SetGraphicsRootDescriptorTable(2, _effectSRVHeap->GetGPUDescriptorHandleForHeapStart());

	_cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
	_cmdList->IASetVertexBuffers(0, 1, &_peraVBV);
	_cmdList->DrawInstanced(4, 1, 0, 0);
}

void Dx12Wrapper::Flip()
{
	auto bbIdx = _swapchain->GetCurrentBackBufferIndex();
	auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(_backBuffers[bbIdx], D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT);
	_cmdList->ResourceBarrier(1, &barrier);

	// 命令のクローズ
	_cmdList->Close();

	// コマンドリストの実行
	ID3D12CommandList* cmdLists[] = { _cmdList.Get() };
	_cmdQueue->ExecuteCommandLists(1, cmdLists);

	_cmdQueue->Signal(_fence.Get(), ++_fenceVal);

	if (_fence->GetCompletedValue() < _fenceVal)
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
	_cmdList->Reset(_cmdAllocator.Get(), nullptr); // 再びコマンドリストをためる準備

	_swapchain->Present(0, 0);
}

void Dx12Wrapper::DrawHorizontalBokeh()
{
	auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(_peraResource2.Get(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_RENDER_TARGET);
	_cmdList->ResourceBarrier(1, &barrier); 

	auto wsize = Application::Instance().GetWindowSize();

	D3D12_VIEWPORT vp = CD3DX12_VIEWPORT(0.0f, 0.0f, static_cast<FLOAT>(wsize.cx), static_cast<FLOAT>(wsize.cy));
	_cmdList->RSSetViewports(1, &vp);

	CD3DX12_RECT rc(0, 0, wsize.cx, wsize.cy);
	_cmdList->RSSetScissorRects(1, &rc);

	auto rtvH = _peraRTVHeap->GetCPUDescriptorHandleForHeapStart();
	rtvH.ptr += _dev->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

	_cmdList->OMSetRenderTargets(1, &rtvH, false, nullptr);

	// 画面のクリア
	float clearColor[] = { 0.5f,0.5f,0.5f,1.0f };
	_cmdList->ClearRenderTargetView(rtvH, clearColor, 0, nullptr);


	_cmdList->SetGraphicsRootSignature(_rootSignature.Get());
	_cmdList->SetPipelineState(_pipeline.Get());

	auto handle = _peraRegisterHeap->GetGPUDescriptorHandleForHeapStart();
	_cmdList->SetDescriptorHeaps(1, _peraRegisterHeap.GetAddressOf());
	_cmdList->SetGraphicsRootDescriptorTable(0, handle);

	handle.ptr += _dev->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV) * 2;
	_cmdList->SetDescriptorHeaps(1, _peraRegisterHeap.GetAddressOf());
	_cmdList->SetGraphicsRootDescriptorTable(1, handle);


	_cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
	_cmdList->IASetVertexBuffers(0, 1, &_peraVBV);
	_cmdList->DrawInstanced(4, 1, 0, 0);
	
	barrier = CD3DX12_RESOURCE_BARRIER::Transition(_peraResource2.Get(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
	_cmdList->ResourceBarrier(1, &barrier);
}


void Dx12Wrapper::BeginDraw()
{


}

void Dx12Wrapper::EndDraw()
{

}

ComPtr<ID3D12Resource> Dx12Wrapper::GetTextureByPath(const char* texpath)
{
	auto it = _textureTable.find(texpath);
	if (it != _textureTable.end())
	{
		return _textureTable[texpath];
	}
	else
	{
		return ComPtr<ID3D12Resource>(CreateTextureFromFile(texpath));
	}
}

ComPtr<ID3D12Device> Dx12Wrapper::Device()
{
	return _dev;
}

ComPtr<ID3D12GraphicsCommandList> Dx12Wrapper::CommandList()
{
	return _cmdList;
}

ComPtr<IDXGISwapChain4> Dx12Wrapper::Swapchain()
{
	return _swapchain;
}

void Dx12Wrapper::SetScene()
{
	ID3D12DescriptorHeap* sceneheaps[] = { _sceneDescHeap.Get() };
	_cmdList->SetDescriptorHeaps(1, sceneheaps);
	_cmdList->SetGraphicsRootDescriptorTable(0, _sceneDescHeap->GetGPUDescriptorHandleForHeapStart());
}
