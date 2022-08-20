#include "Application.h"
#include "Dx12Wrapper.h"
#include "PMDRenderer.h"
#include "PMDActor.h"

// ウィンドウ定数
const unsigned int window_width = 1280;
const unsigned int window_height = 720;

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

void Application::CreateGameWindow(HWND& hwnd, WNDCLASSEX& windowClass)
{
	HINSTANCE hInst = GetModuleHandle(nullptr);

	windowClass.cbSize = sizeof(WNDCLASSEX);
	windowClass.lpfnWndProc = (WNDPROC)WindowProcedure; // コールバック関数の指定
	windowClass.lpszClassName = L"DirectXTest"; // アプリケーションクラス名(適当でよい)
	windowClass.hInstance = GetModuleHandle(0); // ハンドルの取得
	RegisterClassEx(&windowClass); // アプリケーションクラス(ウィンドウクラスの指定をOSに伝える)

	RECT wrc = { 0,0,window_width, window_height }; // ウィンドウサイズを決める
	AdjustWindowRect(&wrc, WS_OVERLAPPEDWINDOW, false); // 関数を使ってウィンドウのサイズを補正する

	// ウィンドウオブジェクトの生成
	hwnd = CreateWindow
	(
		windowClass.lpszClassName, // クラス名指定
		L"DX12テスト", // タイトルバーの文字
		WS_OVERLAPPEDWINDOW, // タイトルバーと境界線があるウィンドウ
		CW_USEDEFAULT, // 表示x座標はＯＳにお任せ
		CW_USEDEFAULT, // 表示y座標はＯＳにお任せ
		wrc.right - wrc.left, // ウィンドウ幅
		wrc.bottom - wrc.top, // ウィンドウ高
		nullptr, // 親ウィンドウハンドル
		nullptr, // メニューハンドル
		windowClass.hInstance, // 呼び出しアプリケーションハンドル
		nullptr // 追加パラメータ
	);
}

Application::Application()
{

}

Application& Application::Instance()
{
	static Application instance;
	return instance;
}

bool Application::Init()
{
	auto result = CoInitializeEx(0, COINIT_MULTITHREADED);
	CreateGameWindow(_hwnd, _windowClass);

	// DirectX12ラッパー生成&初期化
	_dx12.reset(new Dx12Wrapper(_hwnd));
	_pmdRenderer.reset(new PMDRenderer(*_dx12));
	_pmdActor.reset(new PMDActor("Model/初音ミク.pmd", "Motion/swing.vmd", *_pmdRenderer));

	return true;
}

void Application::Run()
{
	ShowWindow(_hwnd, SW_SHOW); // ウィンドウ表示
	float angle = 0.0f;
	MSG msg = {};
	unsigned int frame = 0;

	_pmdActor->PlayAnimation();

	while (true)
	{
		if (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE))
		{
			TranslateMessage(&msg);
			DispatchMessageW(&msg);
		}

		// もうアプリケーションが終わるって時にmessageがWM_QUITになる
		if (msg.message == WM_QUIT)
		{
			break;
		}

		_pmdActor->Update();

		_pmdRenderer->SetupRootSignature();

		_dx12->SetupShadowPass();
		_pmdRenderer->SetupShadowPass();
		_dx12->SetScene();
		_pmdActor->Draw(true);

		_dx12->PreDrawToPera1();
		_pmdRenderer->BeforeDraw();
		_dx12->DrawToPera1();
		_dx12->SetDepthSRV();
		_dx12->SetScene();
		_pmdActor->Draw(false);
		//_pmdRenderer->Draw();
		_dx12->PostDrawToPera1();

		//_dx12->DrawHorizontalBokeh();

		_dx12->Clear();
		_dx12->Draw();
		_dx12->Flip();

		// 全体の描画準備
		//_dx12->BeginDraw();

		//// PMD用の描画パイプラインに合わせる
		//_dx12->CommandList()->SetPipelineState(_pmdRenderer->GetPipelineState());

		//// ルートシグネチャもPMD用に合わせる
		//_dx12->CommandList()->SetGraphicsRootSignature(_pmdRenderer->GetRootSignature());

		//_dx12->CommandList()->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

		//_dx12->SetScene();

		//_pmdActor->Update();
		//_pmdActor->Draw();

		//_dx12->EndDraw();

		//// フリップ
		//_dx12->Swapchain()->Present(1, 0);

	}
}

void Application::Terminate()
{
	UnregisterClass(_windowClass.lpszClassName, _windowClass.hInstance);
}

SIZE Application::GetWindowSize() const
{
	SIZE ret;
	ret.cx = window_width;
	ret.cy = window_height;
	return ret;
}

Application::~Application()
{
}
