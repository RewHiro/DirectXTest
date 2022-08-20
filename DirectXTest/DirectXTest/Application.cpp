#include "Application.h"
#include "Dx12Wrapper.h"
#include "PMDRenderer.h"
#include "PMDActor.h"

// �E�B���h�E�萔
const unsigned int window_width = 1280;
const unsigned int window_height = 720;

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

void Application::CreateGameWindow(HWND& hwnd, WNDCLASSEX& windowClass)
{
	HINSTANCE hInst = GetModuleHandle(nullptr);

	windowClass.cbSize = sizeof(WNDCLASSEX);
	windowClass.lpfnWndProc = (WNDPROC)WindowProcedure; // �R�[���o�b�N�֐��̎w��
	windowClass.lpszClassName = L"DirectXTest"; // �A�v���P�[�V�����N���X��(�K���ł悢)
	windowClass.hInstance = GetModuleHandle(0); // �n���h���̎擾
	RegisterClassEx(&windowClass); // �A�v���P�[�V�����N���X(�E�B���h�E�N���X�̎w���OS�ɓ`����)

	RECT wrc = { 0,0,window_width, window_height }; // �E�B���h�E�T�C�Y�����߂�
	AdjustWindowRect(&wrc, WS_OVERLAPPEDWINDOW, false); // �֐����g���ăE�B���h�E�̃T�C�Y��␳����

	// �E�B���h�E�I�u�W�F�N�g�̐���
	hwnd = CreateWindow
	(
		windowClass.lpszClassName, // �N���X���w��
		L"DX12�e�X�g", // �^�C�g���o�[�̕���
		WS_OVERLAPPEDWINDOW, // �^�C�g���o�[�Ƌ��E��������E�B���h�E
		CW_USEDEFAULT, // �\��x���W�͂n�r�ɂ��C��
		CW_USEDEFAULT, // �\��y���W�͂n�r�ɂ��C��
		wrc.right - wrc.left, // �E�B���h�E��
		wrc.bottom - wrc.top, // �E�B���h�E��
		nullptr, // �e�E�B���h�E�n���h��
		nullptr, // ���j���[�n���h��
		windowClass.hInstance, // �Ăяo���A�v���P�[�V�����n���h��
		nullptr // �ǉ��p�����[�^
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

	// DirectX12���b�p�[����&������
	_dx12.reset(new Dx12Wrapper(_hwnd));
	_pmdRenderer.reset(new PMDRenderer(*_dx12));
	_pmdActor.reset(new PMDActor("Model/�����~�N.pmd", "Motion/swing.vmd", *_pmdRenderer));

	return true;
}

void Application::Run()
{
	ShowWindow(_hwnd, SW_SHOW); // �E�B���h�E�\��
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

		// �����A�v���P�[�V�������I�����Ď���message��WM_QUIT�ɂȂ�
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

		// �S�̂̕`�揀��
		//_dx12->BeginDraw();

		//// PMD�p�̕`��p�C�v���C���ɍ��킹��
		//_dx12->CommandList()->SetPipelineState(_pmdRenderer->GetPipelineState());

		//// ���[�g�V�O�l�`����PMD�p�ɍ��킹��
		//_dx12->CommandList()->SetGraphicsRootSignature(_pmdRenderer->GetRootSignature());

		//_dx12->CommandList()->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

		//_dx12->SetScene();

		//_pmdActor->Update();
		//_pmdActor->Draw();

		//_dx12->EndDraw();

		//// �t���b�v
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
