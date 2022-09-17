#pragma once
#include <d3d12.h>
#include <dxgi1_6.h>
#include <map>
#include <memory>
#include <unordered_map>
#include <DirectXTex.h>
#include <wrl.h>
#include <string>
#include <functional>
#include <array>

class Dx12Wrapper
{
	SIZE _winSize;
	template<typename T>
	using ComPtr = Microsoft::WRL::ComPtr<T>;

	// DXGIまわり
	ComPtr<IDXGIFactory4> _dxgiFactory = nullptr; // DXGIインターフェイス
	ComPtr<IDXGISwapChain4> _swapchain = nullptr; // スワップチェイン

	// DirectX12まわり
	ComPtr<ID3D12Device> _dev = nullptr; // デバイス
	ComPtr<ID3D12CommandAllocator> _cmdAllocator = nullptr; // コマンドアロケータ
	ComPtr<ID3D12GraphicsCommandList> _cmdList = nullptr; // コマンドリスト
	ComPtr<ID3D12CommandQueue> _cmdQueue = nullptr; // コマンドキュー

	// 表示に関わるバッファ周り
	ComPtr<ID3D12Resource> _depthBuffer = nullptr; // 深度バッファー
	std::vector<ID3D12Resource*> _backBuffers; // バックバッファ(2つ以上…スワップチェインが確保)
	ComPtr<ID3D12DescriptorHeap> _rtvHeaps = nullptr; // レンダーターゲット用デスクリプタヒープ
	ComPtr<ID3D12DescriptorHeap> _dsvHeaps = nullptr; // 深度バッファビュー用デスクリプタヒープ
	std::unique_ptr<D3D12_VIEWPORT> _viewport; // ビューポート
	std::unique_ptr<D3D12_RECT> _scissorrect; // シザー矩形

	//ComPtr<ID3D12Resource> _peraResource;
	std::array<ComPtr<ID3D12Resource>, 2> _peraResources;

	ComPtr<ID3D12DescriptorHeap> _peraRTVHeap; // レンダーターゲット用
	ComPtr<ID3D12DescriptorHeap> _peraSRVHeap; // テクスチャ用
	ComPtr<ID3D12Resource> _peraVB;
	D3D12_VERTEX_BUFFER_VIEW _peraVBV;
	ComPtr<ID3D12PipelineState> _pipeline = nullptr; // パイプライン
	ComPtr<ID3D12RootSignature> _rootSignature = nullptr; // ルートシグネチャ
	ComPtr<ID3D12DescriptorHeap> _peraRegisterHeap;

	ComPtr<ID3D12Resource> _bokehParamBuffer;
	ComPtr<ID3D12Resource> _peraResource2;
	ComPtr<ID3D12PipelineState> _pipeline2 = nullptr; // パイプライン

	std::array<ComPtr<ID3D12Resource>, 2> _bloomBuffer; // ブルーム用バッファー
	ComPtr<ID3D12PipelineState> _blurPipeline = nullptr;

	ComPtr<ID3D12Resource> _dofBuffer;; // 被写界深度用ぼかしバッファー

	// 歪みテクスチャ用
	ComPtr<ID3D12DescriptorHeap> _effectSRVHeap;
	ComPtr<ID3D12Resource> _effectTexBuffer;

	// 深度値テクスチャ用
	ComPtr<ID3D12DescriptorHeap> _depthSRVHeap;

	// シャドウマップ用深度バッファー
	ComPtr<ID3D12Resource> _lightDepthBuffer;

	// シーンを構成するバッファまわり
	ComPtr<ID3D12Resource> _sceneConstBuff = nullptr;

	struct SceneData {
		DirectX::XMMATRIX view; // ビュー行列
		DirectX::XMMATRIX proj; // プロジェクション行列
		DirectX::XMMATRIX lightCamera; //ライトから見たビュー
		DirectX::XMMATRIX shadow; // 影
		DirectX::XMFLOAT3 eye; // 視点座標
	};
	SceneData* _mappedSceneData;
	ComPtr<ID3D12DescriptorHeap> _sceneDescHeap = nullptr;
	DirectX::XMFLOAT3 _parallelLightVec;

	// フェンス
	ComPtr<ID3D12Fence> _fence = nullptr;
	UINT64 _fenceVal = 0;

	// 最終的なレンダーターゲットの生成
	HRESULT CreateFinalRenderTargets();
	// デプスステンシルビューの生成
	HRESULT CreateDepthStencilView();

	// スワップチェインの生成
	HRESULT CreateSwapChain(const HWND& hwnd);

	// DXGIまわりの初期化
	HRESULT InitializeDXGIDevice();


	// コマンドまわりの初期化
	HRESULT InitializeCommand();

	// ビュープロジェクションビューの生成
	HRESULT CreateSceneView();

	HRESULT CreatePeraResource();

	HRESULT CreateBokeResource();

	// ロード用テーブル
	using LoadLamda_t = std::function<HRESULT(const std::wstring& path, DirectX::TexMetadata*, DirectX::ScratchImage&)>;
	std::map<std::string, LoadLamda_t> _loadLambdaTable;
	// テクスチャテーブル
	std::unordered_map<std::string, ComPtr<ID3D12Resource>> _textureTable;
	// テクスチャローダテーブル
	void CreateTextureLoaderTable();
	// テクスチャ名からテクスチャバッファ作成、中身をコピー
	ID3D12Resource* CreateTextureFromFile(const char* texpath);

	bool CheckShaderCompileResult(HRESULT result, ID3DBlob* error = nullptr);

	std::vector<float> GetGaussianWeights(size_t count, float s);

	bool CreateeffectBufferAndView();

public:
	Dx12Wrapper(HWND hwnd);
	~Dx12Wrapper();

	void Update();

	void PreDrawToPera1();
	void DrawToPera1();
	void PostDrawToPera1();

	void Clear();
	void Draw();
	void Flip();
	void DrawHorizontalBokeh();
	void DrawShrinkTextureForBlur();
	void SetupShadowPass();
	void SetDepthSRV();

	void BeginDraw();
	void EndDraw();

	// テクスチャパスから必要なテクスチャバッファへのポインタを返す
	// @param texpath テクスチャファイルパス
	ComPtr<ID3D12Resource> GetTextureByPath(const char* texpath);

	ComPtr<ID3D12Device> Device(); // デバイス
	ComPtr<ID3D12GraphicsCommandList> CommandList(); // コマンドリスト
	ComPtr<IDXGISwapChain4> Swapchain(); // スワップチェイン

	void SetScene();
};