#include "PMDRenderer.h"
#include <d3dx12.h>
#include <cassert>
#include <d3dcompiler.h>
#include "Dx12Wrapper.h"
#include <string>
#include <algorithm>

using namespace std;

namespace {
	void PrintErrorBlob(ID3DBlob* blob)
	{
		assert(blob);
		string err;
		err.resize(blob->GetBufferSize());
		copy_n((char*)blob->GetBufferPointer(), err.size(), err.begin());
	}
}

ID3D12Resource* PMDRenderer::CreateDefaultTexture(size_t width, size_t height)
{
	D3D12_RESOURCE_DESC resDesc = CD3DX12_RESOURCE_DESC::Tex2D(DXGI_FORMAT_R8G8B8A8_UNORM, width, height);
	D3D12_HEAP_PROPERTIES texHeapProp = CD3DX12_HEAP_PROPERTIES(D3D12_CPU_PAGE_PROPERTY_WRITE_BACK, D3D12_MEMORY_POOL_L0);
	ID3D12Resource* buff = nullptr;

	auto result = _dx12.Device()->CreateCommittedResource
	(
		&texHeapProp,
		D3D12_HEAP_FLAG_NONE, // 特に指定なし
		&resDesc,
		D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
		nullptr,
		IID_PPV_ARGS(&buff)
	);

	if (FAILED(result))
	{
		assert(SUCCEEDED(result));
		return nullptr;
	}
	return buff;
}

ID3D12Resource* PMDRenderer::CreateWhiteTexture()
{
	ID3D12Resource* whiteBuff = CreateDefaultTexture(4, 4);

	std::vector<unsigned char> data(4 * 4 * 4);
	std::fill(data.begin(), data.end(), 0xff);

	auto result = whiteBuff->WriteToSubresource(0, nullptr, data.data(), 4 * 4, data.size());
	assert(SUCCEEDED(result));

	return whiteBuff;
}

ID3D12Resource* PMDRenderer::CreateBlackTexture()
{
	ID3D12Resource* blackBuff = CreateDefaultTexture(4, 4);

	std::vector<unsigned char> data(4 * 4 * 4);
	std::fill(data.begin(), data.end(), 0x00);

	auto result = blackBuff->WriteToSubresource(0, nullptr, data.data(), 4 * 4, data.size());
	assert(SUCCEEDED(result));

	return blackBuff;
}

ID3D12Resource* PMDRenderer::CreateGrayGradationTexture()
{
	ID3D12Resource* gradBuff = CreateDefaultTexture(4, 4);

	// 上が白くて下が黒いテクスチャデータ
	std::vector<unsigned int> data(4 * 256);
	auto it = data.begin();
	unsigned int c = 0xff;

	for (; it != data.end(); it += 4)
	{
		auto col = (0xff << 24) | RGB(c, c, c); // RGBAが逆並びしているためRGBマクロと0xff<<24を用いて表す。
		std::fill(it, it + 4, col);
		--c;
	}

	auto result = gradBuff->WriteToSubresource(0, nullptr, data.data(), 4 * sizeof(unsigned int), sizeof(unsigned int) * data.size());
	assert(SUCCEEDED(result));

	return gradBuff;
}

HRESULT PMDRenderer::CreateGraphicsPipelineForPMD()
{
	ComPtr<ID3DBlob> vsBlob = nullptr;
	ComPtr<ID3DBlob> psBlob = nullptr;
	ComPtr<ID3DBlob> errorBlob = nullptr;


	auto result = D3DCompileFromFile
	(
		L"BasicVertexShader.hlsl", // シェーダー名
		nullptr, // defineはなし
		D3D_COMPILE_STANDARD_FILE_INCLUDE, // インクルードはデフォルト
		"BasicVS", "vs_5_0", // 関数はBasicVS、対象シェーダーはvs_5_0
		D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION, // デバッグ用および最適化はなし
		0,
		&vsBlob, &errorBlob // エラー時はerrorBlobにメッセージが入る
	);

	if (!CheckShaderCompileResult(result, errorBlob.Get()))
	{
		assert(0);
		return result;
	}

	result = D3DCompileFromFile
	(
		L"BasicPixelShader.hlsl", // シェーダー名
		nullptr, // defineはなし
		D3D_COMPILE_STANDARD_FILE_INCLUDE, // インクルードはデフォルト
		"BasicPS", "ps_5_0", // 関数はBasicPS、対象シェーダーはps_5_0
		D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION, // デバッグ用および最適化はなし
		0,
		&psBlob, &errorBlob // エラー時はerrorBlobにメッセージが入る
	);

	if (!CheckShaderCompileResult(result, errorBlob.Get()))
	{
		assert(0);
		return result;
	}

	D3D12_INPUT_ELEMENT_DESC inputlayout[] =
	{
		{ // 座標情報
			"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0,
			D3D12_APPEND_ALIGNED_ELEMENT,
			D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0
		},
		{ // 法線情報
			"NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0,
			D3D12_APPEND_ALIGNED_ELEMENT,
			D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0
		},
		{ // uv
			"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0,
			D3D12_APPEND_ALIGNED_ELEMENT,
			D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0
		},
		{ // 
			"BONE_NO", 0, DXGI_FORMAT_R16G16_UINT, 0,
			D3D12_APPEND_ALIGNED_ELEMENT,
			D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0
		},
		{ // 
			"WEIGHT", 0, DXGI_FORMAT_R8_UINT, 0,
			D3D12_APPEND_ALIGNED_ELEMENT,
			D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0
		},
		//{ // 
		//	"EDGE_FLG", 0, DXGI_FORMAT_R8_UINT, 0,
		//	D3D12_APPEND_ALIGNED_ELEMENT,
		//	D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0
		//},
	};

	D3D12_GRAPHICS_PIPELINE_STATE_DESC gpipeline = {};
	gpipeline.pRootSignature = _rootSignature.Get();

	gpipeline.VS = CD3DX12_SHADER_BYTECODE(vsBlob.Get());
	gpipeline.PS = CD3DX12_SHADER_BYTECODE(psBlob.Get());

	// デフォルトのサンプルマスクを表す定数(0xffffffff)
	gpipeline.SampleMask = D3D12_DEFAULT_SAMPLE_MASK;

	// まだアンチエイリアスは使わないためfalse
	gpipeline.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
	gpipeline.RasterizerState.CullMode = D3D12_CULL_MODE_NONE; // カリングしない

	gpipeline.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);

	gpipeline.DepthStencilState.DepthEnable = true; // 深度バッファーを使う
	gpipeline.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL; // 書き込む
	gpipeline.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS; // 小さいほうを採用
	gpipeline.DSVFormat = DXGI_FORMAT_D32_FLOAT;
	gpipeline.DepthStencilState.StencilEnable = false;

	gpipeline.InputLayout.pInputElementDescs = inputlayout; // レイアウト先頭アドレス
	gpipeline.InputLayout.NumElements = _countof(inputlayout); // レイアウト配列の要素数

	gpipeline.IBStripCutValue = D3D12_INDEX_BUFFER_STRIP_CUT_VALUE_DISABLED; // カットなし

	// 三角形で構成
	gpipeline.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;

	gpipeline.NumRenderTargets = 3; // 今は1つのみ

	gpipeline.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM; // 0～1に正規化されたRGBA
	gpipeline.RTVFormats[1] = DXGI_FORMAT_R8G8B8A8_UNORM; // 0～1に正規化されたRGBA
	gpipeline.RTVFormats[2] = DXGI_FORMAT_R8G8B8A8_UNORM; // 0～1に正規化されたRGBA

	gpipeline.SampleDesc.Count = 1; // サンプリングは1ピクセルにつき
	gpipeline.SampleDesc.Quality = 0; // クオリティは最低

	result = _dx12.Device()->CreateGraphicsPipelineState(&gpipeline, IID_PPV_ARGS(_pipeline.ReleaseAndGetAddressOf()));

	if (FAILED(result))
	{
		assert(SUCCEEDED(result));
	}

	result = D3DCompileFromFile
	(
		L"BasicVertexShader.hlsl", // シェーダー名
		nullptr, // defineはなし
		D3D_COMPILE_STANDARD_FILE_INCLUDE, // インクルードはデフォルト
		"shadowVS", "vs_5_0", // 関数はshadowVS、対象シェーダーはvs_5_0
		D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION, // デバッグ用および最適化はなし
		0,
		vsBlob.ReleaseAndGetAddressOf(), errorBlob.ReleaseAndGetAddressOf() // エラー時はerrorBlobにメッセージが入る
	);

	if (!CheckShaderCompileResult(result, errorBlob.Get()))
	{
		assert(0);
		return result;
	}

	gpipeline.VS = CD3DX12_SHADER_BYTECODE(vsBlob.Get()); // 頂点シェーダー設定
	gpipeline.PS.BytecodeLength = 0; // ピクセルシェーダー必要なし
	gpipeline.PS.pShaderBytecode = nullptr; // ピクセルシェーダー必要なし
	gpipeline.NumRenderTargets = 0; // レンダーターゲット必要なし
	gpipeline.RTVFormats[0] = DXGI_FORMAT_UNKNOWN; // レンダーターゲット必要なし
	gpipeline.RTVFormats[1] = DXGI_FORMAT_UNKNOWN; // レンダーターゲット必要なし
	gpipeline.RTVFormats[2] = DXGI_FORMAT_UNKNOWN; // レンダーターゲット必要なし

	result = _dx12.Device()->CreateGraphicsPipelineState(&gpipeline, IID_PPV_ARGS(_plsShadow.ReleaseAndGetAddressOf()));

	if (FAILED(result)) {
		// エラー処理
		assert(SUCCEEDED(result));
	}


	return result;
}

HRESULT PMDRenderer::CreateRootSignature()
{
	CD3DX12_DESCRIPTOR_RANGE descTblRange[5] = {}; // テクスチャと定数の2つ
	descTblRange[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, 0);  // 定数[b0](ビュープロジェクション用)
	descTblRange[1].Init(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, 1);  // 定数[b1](ワールド、ボーン用)
	descTblRange[2].Init(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, 2);  // 定数[b2](マテリアル用)
	descTblRange[3].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 4, 0);  // テクスチャ4つ
	descTblRange[4].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 4);


	CD3DX12_ROOT_PARAMETER rootparam[4] = {};

	rootparam[0].InitAsDescriptorTable(1, &descTblRange[0]); // ビュープロジェクション変換
	rootparam[1].InitAsDescriptorTable(1, &descTblRange[1]); // ワールド・ボーン変換
	rootparam[2].InitAsDescriptorTable(2, &descTblRange[2]); // マテリアル周り
	rootparam[3].InitAsDescriptorTable(1, &descTblRange[4]);


	CD3DX12_STATIC_SAMPLER_DESC samplerDesc[3] = {};
	samplerDesc[0].Init(0);
	samplerDesc[1].Init(1, D3D12_FILTER_ANISOTROPIC, D3D12_TEXTURE_ADDRESS_MODE_CLAMP, D3D12_TEXTURE_ADDRESS_MODE_CLAMP);

	samplerDesc[2].Init
	(
		2
		,D3D12_FILTER_COMPARISON_MIN_MAG_MIP_LINEAR
		,D3D12_TEXTURE_ADDRESS_MODE_CLAMP
		,D3D12_TEXTURE_ADDRESS_MODE_CLAMP
		,D3D12_TEXTURE_ADDRESS_MODE_CLAMP
		,0.0f
		,1
	);

	CD3DX12_ROOT_SIGNATURE_DESC rootSignatureDesc = {};
	rootSignatureDesc.Init(4, rootparam, 3, samplerDesc, D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

	ComPtr<ID3DBlob> rootSigBlob = nullptr;
	ComPtr<ID3DBlob> errorBlob = nullptr;

	auto result = D3D12SerializeRootSignature
	(
		&rootSignatureDesc, // ルートシグネチャ設定
		D3D_ROOT_SIGNATURE_VERSION_1, // ルートシグネチャバージョン
		&rootSigBlob, // シェーダーを作ったときと同じ
		&errorBlob // エラー処理も同じ
	);

	result = _dx12.Device()->CreateRootSignature
	(
		0, // nodemask。0でよい
		rootSigBlob->GetBufferPointer(), // シェーダーのときと同様
		rootSigBlob->GetBufferSize(), // シェーダーのときと同様
		IID_PPV_ARGS(_rootSignature.ReleaseAndGetAddressOf())
	);

	if (FAILED(result))
	{
		assert(SUCCEEDED(result));
		return result;
	}

	return result;
}

bool PMDRenderer::CheckShaderCompileResult(HRESULT result, ID3DBlob* error)
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

PMDRenderer::PMDRenderer(Dx12Wrapper& dx12):_dx12(dx12)
{
	assert(SUCCEEDED(CreateRootSignature()));
	assert(SUCCEEDED(CreateGraphicsPipelineForPMD()));
	_whiteTex = CreateWhiteTexture();
	_blackTex = CreateBlackTexture();
	_gradTex = CreateGrayGradationTexture();
}

PMDRenderer::~PMDRenderer()
{
}

void PMDRenderer::Update()
{
}

void PMDRenderer::BeforeDraw()
{
	auto cmdlist = _dx12.CommandList();
	cmdlist->SetPipelineState(_pipeline.Get());
}

void PMDRenderer::Draw()
{

}

void PMDRenderer::SetupRootSignature()
{
	auto cmdlist = _dx12.CommandList();
	cmdlist->SetGraphicsRootSignature(_rootSignature.Get());
	cmdlist->IASetPrimitiveTopology(D3D10_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

}

void PMDRenderer::SetupShadowPass()
{
	auto cmdlist = _dx12.CommandList();
	cmdlist->SetPipelineState(_plsShadow.Get());
}

ID3D12PipelineState* PMDRenderer::GetPipelineState()
{
	return _pipeline.Get();
}

ID3D12RootSignature* PMDRenderer::GetRootSignature()
{
	return _rootSignature.Get();
}
