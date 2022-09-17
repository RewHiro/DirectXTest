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

	// 頂点関連
	ComPtr<ID3D12Resource> _vb = nullptr;
	ComPtr<ID3D12Resource> _ib = nullptr;
	D3D12_VERTEX_BUFFER_VIEW _vbView = {};
	D3D12_INDEX_BUFFER_VIEW _ibView = {};

	ComPtr<ID3D12Resource> _transformMat = nullptr; // 座標変換行列(今はワールドのみ)
	ComPtr<ID3D12DescriptorHeap> _transformHeap = nullptr; // 座標変換ヒープ

	// シェーダー側に投げられるマテリアルデータ
	struct MaterialForHlsl
	{
		DirectX::XMFLOAT3 diffuse; // ディフューズ色
		float alpha; // ディフューズα
		DirectX::XMFLOAT3 specular; // スペキュラ色
		float specularity; // スペキュラの強さ(乗算値)
		DirectX::XMFLOAT3 ambient; // アンビエント色
	};

	// それ以外のマテリアルデータ
	struct AdditionalMaterial
	{
		std::string texPath; // テクスチャファイルポス
		int toonIdx; // トゥーン番号
		bool edgeFlg; // マテリアルごとの輪郭線フラグ
	};

	struct Material
	{
		unsigned int indicesNum; // インデックス数
		MaterialForHlsl material;
		AdditionalMaterial additional;
	};

	struct Transform 
	{
		// 内部に持ってるXMMATRIXメンバが16バイトアライメントであるため
		// Transformをnewする際には16バイト境界に確保する
		void* operator new(size_t size);
		DirectX::XMMATRIX world;
	};

	struct BoneNode
	{
		int boneIdx; // ボーンインデックス
		uint32_t boneType; // ボーン種別
		uint32_t ikParentBone; // IK親ボーン
		DirectX::XMFLOAT3 startPos; // ボーン基準点(回転の中心)
		//DirectX::XMFLOAT3 endPos; // ボーン先端点(実際のスキニングには利用しない)
		std::vector<BoneNode*> children; // 子ノード
	};

	// モーション構造体
	struct KeyFrame
	{
		unsigned int frameNo; // アニメーション開始からのフレーム数
		DirectX::XMVECTOR quaternion; // クォータニオン
		DirectX::XMFLOAT3 offset; // IKの初期座標からのオフセット情報
		DirectX::XMFLOAT2 p1, p2; // ベジェ曲線の中間コントロールポイント

		KeyFrame
		(
			unsigned int fno
			, const DirectX::XMVECTOR& q
			, const DirectX::XMFLOAT3& ofst
			, const DirectX::XMFLOAT2& ip1
			, const DirectX::XMFLOAT2& ip2
		)
			: frameNo(fno)
			, quaternion(q) 
			, offset(ofst)
			, p1(ip1)
			, p2(ip2)
		{}
	};

	struct PMDIK
	{
		uint16_t boneIdx; // IK対象のボーンを示す
		uint16_t targetIdx; // ターゲットに近づけるためのインデックス
		uint16_t iterations; // 試行回数
		float limit; // 1回あたりの回転制限
		std::vector<uint16_t> nodeIdxes; // 間のノード番号
	};

	// IKオン / オフデータ
	struct VMDIKEnable
	{
		// キーフレームがあるフレーム番号
		uint32_t frameNo;

		// 名前とオン / オフフラグのマップ
		std::unordered_map<std::string, bool> ikEnableTable;
	};

	Transform _transform;
	Transform* _mappedTransform = nullptr;
	DirectX::XMMATRIX* _mappedMatrices = nullptr;
	ComPtr<ID3D12Resource> _transformBuff = nullptr;

	// マテリアル関連
	std::vector<Material> _materials;
	ComPtr<ID3D12Resource> _materialBuff = nullptr;
	std::vector<ComPtr<ID3D12Resource>> _textureResources;
	std::vector<ComPtr<ID3D12Resource>> _sphResources;
	std::vector<ComPtr<ID3D12Resource>> _spaResources;
	std::vector<ComPtr<ID3D12Resource>> _toonResources;

	std::vector<DirectX::XMMATRIX> _boneMatrices;
	std::map<std::string, BoneNode> _boneNodeTable;
	std::unordered_map<std::string, std::vector<KeyFrame>> _motiondata;
	std::vector<PMDIK> _ikData;
	std::vector<uint32_t> _knessIdxes;
	std::vector<VMDIKEnable> _ikEnableData;

	// インデックスから名前を検索しやすいように
	std::vector<std::string> _boneNameArray;

	// インデックスからノードを検索しやすいように
	std::vector<BoneNode*> _boneNodeAddressArray;

	unsigned int _indexNum;

	// 読み込んだマテリアルをもとにマテリアルバッファを作成
	HRESULT CreateMaterialData();

	ComPtr<ID3D12DescriptorHeap> _materialHeap = nullptr; // マテリアルヒープ(5個ぶん)
	// マテリアル&テクスチャビューを作成
	HRESULT CreateMaterialAndTextureView();

	// 座標変換用ビューの生成
	HRESULT CreateTransformView();

	// PMDファイルのロード
	HRESULT LoadPMDFile(const char* path);

	// VMDファイルのロード
	HRESULT LoadVMDFile(const char* path);

	void RecursiveMatrixMultiply(BoneNode* node, const DirectX::XMMATRIX& mat);

	void MotionUpdate();

	float GetYFromXOnBezier(float x, DirectX::XMFLOAT2& a, DirectX::XMFLOAT2& b, uint8_t n);

	// CCD-IKによりボーン方向を解決
	// @param ik 対象IKオブジェクト
	void SolveCCDIK(const PMDIK& ik);

	// 余弦定理IKによりボーン方向を解決
	// @param ik 対象IKオブジェクト
	void SolveCosineIK(const PMDIK& ik);

	// LookAt行列によりボーン方向を解決
	// @param ik 対象IKオブジェクト
	void SolveLookAt(const PMDIK& ik);

	void IKSolve(unsigned int  frameNo);

	// z軸を特定の方向に向ける行列を返す関数
	// @param lookat 向かせたい方向ベクトル
	// @param up 上ベクトル
	// @param right 右ベクトル
	DirectX::XMMATRIX LookAtMatrix(const DirectX::XMVECTOR& lookat, const DirectX::XMFLOAT3& up, const DirectX::XMFLOAT3& right);

	// 特定のベクトルを特定の方向に向けるための行列を返す
	// @param origin 特定のベクトル
	// @param lookat 向かせたい方向ベクトル
	// @param up 上ベクトル
	// @param right 右ベクトル
	// @retval 特定のベクトルを特定の方向に向けるための行列
	DirectX::XMMATRIX LookAtMatrix(const DirectX::XMVECTOR& origin, const DirectX::XMVECTOR& lookat, const DirectX::XMFLOAT3& up, const DirectX::XMFLOAT3& right);

	float _angle; // テスト用Y軸回転
	DWORD _startTime; // アニメーション開始時のミリ秒
	DWORD _duration;

public:
	PMDActor(const char* filepath, const char* vmdFilePath, PMDRenderer& renderer, const DirectX::XMFLOAT3& pos);
	~PMDActor();

	// クローンは頂点およびマテリアルは共通のバッファを見るようにする
	PMDActor* Clone();
	void Update();
	void Draw(bool isShadow);
	void PlayAnimation();
};