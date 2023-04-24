#include "../PostureBarMod.hpp"
#include "Logger.hpp"
#include "D3DRenderer.hpp"
#include "Hooking.hpp"
#include "PostureBarUI.hpp"

IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

namespace ER 
{
	D3DRenderer::ProgramData::ProgramData()
	{
		Logger::log("Acquiring program data");

		m_GamePid = GetCurrentProcessId();
		m_GameHandle = GetCurrentProcess();
		m_GameWindow = FindWindowExW(NULL, NULL, L"ELDEN RING�", NULL);
		
		if (m_GameWindow == NULL)
		{
			Logger::log("Failed to find \"ELDEN RING�\" window, trying to get via GetForegroundWindow", LogLevel::Warning);
			m_GameWindow = GetForegroundWindow();
		}

		m_ModuleBase = (uintptr_t)GetModuleHandle(NULL);

		RECT TempRect;
		GetWindowRect(m_GameWindow, &TempRect);
		m_GameWidth = TempRect.right - TempRect.left;
		m_GameHeight = TempRect.bottom - TempRect.top;

		char TempTitle[MAX_PATH];
		GetWindowText(m_GameWindow, TempTitle, sizeof(TempTitle));
		m_GameTitle = TempTitle;

		char TempClassName[MAX_PATH];
		GetClassName(m_GameWindow, TempClassName, sizeof(TempClassName));
		m_ClassName = TempClassName;

		char TempPath[MAX_PATH];
		GetModuleFileNameEx(m_GameHandle, NULL, TempPath, sizeof(TempPath));
		m_GamePath = TempPath;

		Logger::log("Program Data:");
		Logger::log("\tGame Pid: " + std::to_string(m_GamePid));
		Logger::log("\tGame Module Base: " + std::to_string(m_ModuleBase));
		Logger::log("\tGame Title: " + std::string(m_GameTitle));
		Logger::log("\tGame Class Name: " + std::string(m_ClassName));
		Logger::log("\tGame Game Path: " + std::string(m_GamePath));
		Logger::log("\tGame Window Width: " + std::to_string(m_GameWidth));
		Logger::log("\tGame Window Height: " + std::to_string(m_GameHeight));
		Logger::log("\tIs Window Class Unicode: " + std::to_string(IsWindowUnicode(m_GameWindow)));
	}

	//-----------------------------------------------------------------------------------
	//									    D3DWindow
	//-----------------------------------------------------------------------------------
	static uint64_t* MethodsTable = NULL;

	bool D3DRenderer::Hook()
	{
		Logger::log("Hooking D3DWindow");
		if (InitHook()) 
		{
			Logger::log("Hooking ExecuteCommandLists");
			CreateHook(54, (void**)&oExecuteCommandLists, HookExecuteCommandLists);
			Logger::log("Hooking Present");
			CreateHook(140, (void**)&oPresent, HookPresent);
			Logger::log("Hooking ResizeTarget");
			CreateHook(146, (void**)&oResizeTarget, HookResizeTarget);
			return 1;
		}

		Logger::log("Failed to init D3DWindow hooking", LogLevel::Error);
		return 0;
	}

	void D3DRenderer::Unhook()
	{
		Logger::log("Unhooking D3DWindow");
		SetWindowLongPtr(programData->m_GameWindow, GWLP_WNDPROC, (LONG_PTR)m_OldWndProc);
		DisableAll();
	}

    bool D3DRenderer::InitHook()
    {
#ifdef _DEBUG
		try
		{
#endif // _DEBUG
		Logger::log("Init D3DWindow");
		if (!InitWindow()) 
		{
			Logger::log("Failed to init D3DWindow", LogLevel::Error);
			return false;
		}

		Logger::log("Getting D3D12 and DXGI modules");
		HMODULE D3D12Module = GetModuleHandle("d3d12.dll");
		HMODULE DXGIModule = GetModuleHandle("dxgi.dll");
		if (D3D12Module == NULL || DXGIModule == NULL) 
		{
			Logger::log("Failed to get DX modules: D3D12Module - " + std::to_string(D3D12Module != NULL) + "DXGIModule - " + std::to_string(DXGIModule != NULL), LogLevel::Error);
			DeleteWindow();
			return false;
		}

		Logger::log("Getting DXGI factory");
		void* CreateDXGIFactory = GetProcAddress(DXGIModule, "CreateDXGIFactory");
		if (CreateDXGIFactory == NULL) 
		{
			Logger::log("Failed to get DXGI factory", LogLevel::Error);
			DeleteWindow();
			return false;
		}

		Logger::log("Init DXGI factory");
		IDXGIFactory* Factory;
		if (((long(__stdcall*)(const IID&, void**))(CreateDXGIFactory))(__uuidof(IDXGIFactory), (void**)&Factory) < 0) 
		{
			Logger::log("Failed to init DXGI factory", LogLevel::Error);
			DeleteWindow();
			return false;
		}

		Logger::log("Getting IDXGI adapter");
		IDXGIAdapter* Adapter;
		if (Factory->EnumAdapters(0, &Adapter) == DXGI_ERROR_NOT_FOUND) 
		{
			Logger::log("Failed to get IDXGI adapter", LogLevel::Error);
			DeleteWindow();
			return false;
		}

		Logger::log("Getting D3D12CreateDevice process");
		void* D3D12CreateDevice = GetProcAddress(D3D12Module, "D3D12CreateDevice");
		if (D3D12CreateDevice == NULL) 
		{
			Logger::log("Failed to get D3D12CreateDevice process", LogLevel::Error);
			DeleteWindow();
			return false;
		}

		Logger::log("Init ID3D12Device");
		ID3D12Device* Device;
		if (((long(__stdcall*)(IUnknown*, D3D_FEATURE_LEVEL, const IID&, void**))(D3D12CreateDevice))(Adapter, D3D_FEATURE_LEVEL_11_0, __uuidof(ID3D12Device), (void**)&Device) < 0) 
		{
			Logger::log("Failed to init ID3D12Device", LogLevel::Error);
			DeleteWindow();
			return false;
		}

		D3D12_COMMAND_QUEUE_DESC QueueDesc;
		QueueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
		QueueDesc.Priority = 0;
		QueueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
		QueueDesc.NodeMask = 0;

		Logger::log("Init ID3D12Device command queue");
		ID3D12CommandQueue* CommandQueue;
		if (Device->CreateCommandQueue(&QueueDesc, __uuidof(ID3D12CommandQueue), (void**)&CommandQueue) < 0) 
		{
			Logger::log("Failed to init ID3D12Device command queue", LogLevel::Error);
			DeleteWindow();
			return false;
		}

		Logger::log("Init ID3D12Device command allocator");
		ID3D12CommandAllocator* CommandAllocator;
		if (Device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, __uuidof(ID3D12CommandAllocator), (void**)&CommandAllocator) < 0) 
		{
			Logger::log("Failed to init ID3D12Device command allocator", LogLevel::Error);
			DeleteWindow();
			return false;
		}

		Logger::log("Init ID3D12Device command list");
		ID3D12GraphicsCommandList* CommandList;
		if (Device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, CommandAllocator, NULL, __uuidof(ID3D12GraphicsCommandList), (void**)&CommandList) < 0) 
		{
			Logger::log("Failed to init ID3D12Device command list", LogLevel::Error);
			DeleteWindow();
			return false;
		}

		DXGI_RATIONAL RefreshRate;
		RefreshRate.Numerator = 60;
		RefreshRate.Denominator = 1;

		DXGI_MODE_DESC BufferDesc;
		BufferDesc.Width = 100;
		BufferDesc.Height = 100;
		BufferDesc.RefreshRate = RefreshRate;
		BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		BufferDesc.ScanlineOrdering = DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED;
		BufferDesc.Scaling = DXGI_MODE_SCALING_UNSPECIFIED;

		DXGI_SAMPLE_DESC SampleDesc;
		SampleDesc.Count = 1;
		SampleDesc.Quality = 0;

		DXGI_SWAP_CHAIN_DESC SwapChainDesc = {};
		SwapChainDesc.BufferDesc = BufferDesc;
		SwapChainDesc.SampleDesc = SampleDesc;
		SwapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
		SwapChainDesc.BufferCount = 2;
		SwapChainDesc.OutputWindow = WindowHwnd;
		SwapChainDesc.Windowed = 1;
		SwapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
		SwapChainDesc.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;

		Logger::log("Init ID3D12Device swap chain");
		IDXGISwapChain* SwapChain;
		if (Factory->CreateSwapChain(CommandQueue, &SwapChainDesc, &SwapChain) < 0) 
		{
			Logger::log("Failed to init ID3D12Device swap chain", LogLevel::Error);
			DeleteWindow();
			return false;
		}

		Logger::log("Allock D3DWindow method tables");
		MethodsTable = (uint64_t*)::calloc(150, sizeof(uint64_t));
		memcpy(MethodsTable, *(uint64_t**)Device, 44 * sizeof(uint64_t));
		memcpy(MethodsTable + 44, *(uint64_t**)CommandQueue, 19 * sizeof(uint64_t));
		memcpy(MethodsTable + 44 + 19, *(uint64_t**)CommandAllocator, 9 * sizeof(uint64_t));
		memcpy(MethodsTable + 44 + 19 + 9, *(uint64_t**)CommandList, 60 * sizeof(uint64_t));
		memcpy(MethodsTable + 44 + 19 + 9 + 60, *(uint64_t**)SwapChain, 18 * sizeof(uint64_t));
		Sleep(1000);

		MH_Initialize();
		Logger::log("Releasing D3DWindow");
		Device->Release();
		Device = NULL;
		CommandQueue->Release();
		CommandQueue = NULL;
		CommandAllocator->Release();
		CommandAllocator = NULL;
		CommandList->Release();
		CommandList = NULL;
		SwapChain->Release();
		SwapChain = NULL;
		DeleteWindow();
#ifdef _DEBUG
		}
		catch (const std::exception& e)
		{
			Logger::useLogger = true;
			Logger::log(e.what(), LogLevel::Error);
			throw;
		}
		catch (...)
		{
			Logger::useLogger = true;
			Logger::log("Unknown exception during D3DRenderer::InitHook", LogLevel::Error);
			throw;
		}
#endif // _DEBUG
		return true;
    }

	void D3DRenderer::Overlay(IDXGISwapChain* pSwapChain)
	{
#ifdef _DEBUG
		try
		{
#endif // _DEBUG
		if (m_CommandQueue == nullptr)
			return;

		ID3D12CommandQueue* pCmdQueue = this->m_CommandQueue;

		IDXGISwapChain3* pSwapChain3 = nullptr;
		DXGI_SWAP_CHAIN_DESC sc_desc;
		pSwapChain->QueryInterface(IID_PPV_ARGS(&pSwapChain3));
		if (pSwapChain3 == nullptr)
		{
			Logger::log("Failed to query interface for IDXGISwapChain3", LogLevel::Debug);
			return;
		}

		pSwapChain3->GetDesc(&sc_desc);

		if (!m_Init)
		{
			UINT bufferIndex = pSwapChain3->GetCurrentBackBufferIndex();
			ID3D12Device* pDevice;
			if (pSwapChain3->GetDevice(IID_PPV_ARGS(&pDevice)) != S_OK)
			{
				Logger::log("Failed to get device for ID3D12Device", LogLevel::Warning);
				return;
			}

			m_BuffersCounts = sc_desc.BufferCount;

			m_RenderTargets.clear();

			{
				D3D12_DESCRIPTOR_HEAP_DESC desc = {};
				desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
				desc.NumDescriptors = 1;
				desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
				if (pDevice->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&m_DescriptorHeap)) != S_OK)
				{
					pDevice->Release();
					pSwapChain3->Release();
					Logger::log("Failed to create descriptor heap type D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV", LogLevel::Warning);
					return;
				}
			}
			{
				D3D12_DESCRIPTOR_HEAP_DESC desc = {};
				desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
				desc.NumDescriptors = m_BuffersCounts;
				desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
				desc.NodeMask = 1;
				if (pDevice->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&m_rtvDescriptorHeap)) != S_OK)
				{
					pDevice->Release();
					pSwapChain3->Release();
					m_DescriptorHeap->Release();
					Logger::log("Failed to create descriptor heap type D3D12_DESCRIPTOR_HEAP_TYPE_RTV", LogLevel::Warning);
					return;
				}

				SIZE_T rtvDescriptorSize = pDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
				D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = m_rtvDescriptorHeap->GetCPUDescriptorHandleForHeapStart();
				m_CommandAllocator = new ID3D12CommandAllocator * [m_BuffersCounts];
				for (int i = 0; i < m_BuffersCounts; ++i)
				{
					m_RenderTargets.push_back(rtvHandle);
					rtvHandle.ptr += rtvDescriptorSize;
				}
			}

			for (UINT i = 0; i < sc_desc.BufferCount; ++i)
			{
				if (pDevice->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&m_CommandAllocator[i])) != S_OK)
				{
					pDevice->Release();
					pSwapChain3->Release();
					m_DescriptorHeap->Release();
					for (UINT j = 0; j < i; ++j)
					{
						m_CommandAllocator[j]->Release();
					}
					m_rtvDescriptorHeap->Release();
					delete[]m_CommandAllocator;
					Logger::log("Failed to create command allocator D3D12_COMMAND_LIST_TYPE_DIRECT buffer idx: " + std::to_string(i), LogLevel::Warning);
					return;
				}
			}

			if (pDevice->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, m_CommandAllocator[0], NULL, IID_PPV_ARGS(&m_CommandList)) != S_OK ||
				m_CommandList->Close() != S_OK)
			{
				pDevice->Release();
				pSwapChain3->Release();
				m_DescriptorHeap->Release();
				for (UINT i = 0; i < m_BuffersCounts; ++i)
					m_CommandAllocator[i]->Release();
				m_rtvDescriptorHeap->Release();
				delete[]m_CommandAllocator;
				Logger::log("Failed to create command list D3D12_COMMAND_LIST_TYPE_DIRECT", LogLevel::Warning);
				return;
			}

			m_BackBuffer = new ID3D12Resource * [m_BuffersCounts];
			for (UINT i = 0; i < m_BuffersCounts; i++)
			{
				pSwapChain3->GetBuffer(i, IID_PPV_ARGS(&m_BackBuffer[i]));
				pDevice->CreateRenderTargetView(m_BackBuffer[i], NULL, m_RenderTargets[i]);
			}

			Logger::log("ImGui CreateContext");
			ImGui::CreateContext();
			ImGuiIO& io = ImGui::GetIO();
			io.IniFilename = NULL;

			programData->m_GameWindow = FindWindowExW(NULL, NULL, L"ELDEN RING�", NULL);
			if (programData->m_GameWindow == NULL)
			{
				Logger::log("Failed to find \"ELDEN RING�\" window, trying to get via GetForegroundWindow", LogLevel::Warning);
				programData->m_GameWindow = GetForegroundWindow();
			}

			Logger::log("Init ImGui for found window");
			if (!ImGui_ImplWin32_Init(programData->m_GameWindow))
			{
				Logger::log("Failed to init ImGui for found window", LogLevel::Warning);
				return;
			}

			Logger::log("Init ImGui for DX12");
			if (!ImGui_ImplDX12_Init(pDevice, m_BuffersCounts, DXGI_FORMAT_R8G8B8A8_UNORM, NULL, m_DescriptorHeap->GetCPUDescriptorHandleForHeapStart(), m_DescriptorHeap->GetGPUDescriptorHandleForHeapStart()))
			{
				Logger::log("Failed to init ImGui for DX12", LogLevel::Warning);
				return;
			}

			Logger::log("ImGui DX12 create device object");
			if (!ImGui_ImplDX12_CreateDeviceObjects())
			{
				Logger::log("Failed to create ImGui DX12 device object", LogLevel::Warning);
				return;
			}

			ImGui::GetIO().ImeWindowHandle = programData->m_GameWindow;
			m_OldWndProc = SetWindowLongPtr(programData->m_GameWindow, GWLP_WNDPROC, (LONG_PTR)WndProc);

			m_Init = true;

			pDevice->Release();

			Logger::log("ImGui init successful!");
		}

		Logger::log("ImGui create new farme", LogLevel::Debug);
		ImGui_ImplDX12_NewFrame();
		ImGui_ImplWin32_NewFrame();
		ImGui::NewFrame();

		Logger::log("Draw posture bar UI", LogLevel::Debug);
		g_postureUI->Draw();

		Logger::log("ImGui end frame", LogLevel::Debug);
		ImGui::EndFrame();

		UINT bufferIndex = pSwapChain3->GetCurrentBackBufferIndex();

		D3D12_RESOURCE_BARRIER barrier = {};
		barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
		barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
		barrier.Transition.pResource = m_BackBuffer[bufferIndex];
		barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
		barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
		barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;

		m_CommandAllocator[bufferIndex]->Reset();
		m_CommandList->Reset(m_CommandAllocator[bufferIndex], NULL);
		m_CommandList->ResourceBarrier(1, &barrier);
		m_CommandList->OMSetRenderTargets(1, &m_RenderTargets[bufferIndex], FALSE, NULL);
		m_CommandList->SetDescriptorHeaps(1, &m_DescriptorHeap);

		Logger::log("ImGui render", LogLevel::Debug);
		ImGui::Render();
		ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), m_CommandList);

		barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
		barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;
		m_CommandList->ResourceBarrier(1, &barrier);
		m_CommandList->Close();

		Logger::log("ID3D12CommandList execute command list", LogLevel::Debug);
		pCmdQueue->ExecuteCommandLists(1, (ID3D12CommandList**)&m_CommandList);

		Logger::log("IDXGISwapChain3 release", LogLevel::Debug);
		pSwapChain3->Release();

#ifdef _DEBUG
		}
		catch (const std::exception& e)
		{
			Logger::useLogger = true;
			Logger::log(e.what(), LogLevel::Error);
			throw;
		}
		catch (...)
		{
			Logger::useLogger = true;
			Logger::log("Unknown exception during D3DRenderer::Overlay", LogLevel::Error);
			throw;
		}
#endif // _DEBUG
	}

	HRESULT APIENTRY D3DRenderer::HookResizeTarget(IDXGISwapChain* _this, const DXGI_MODE_DESC* pNewTargetParameters)
	{
		Logger::log("Enter HookResizeTarget", LogLevel::Debug);
		g_D3DRenderer->ResetRenderState();
		return g_D3DRenderer->oResizeTarget(_this, pNewTargetParameters);
	}

	LRESULT D3DRenderer::WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
	{
		if (ImGui::GetCurrentContext())
			ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam);

		return CallWindowProc((WNDPROC)g_D3DRenderer->m_OldWndProc, hWnd, msg, wParam, lParam);
	}

    HRESULT APIENTRY D3DRenderer::HookPresent(IDXGISwapChain* pSwapChain, UINT SyncInterval, UINT Flags)
    {
		Logger::log("Enter HookPresent", LogLevel::Debug);
		if (g_KillSwitch) 
		{
			Logger::log("Kill switch");
			g_Hooking->Unhook();
			g_D3DRenderer->oPresent(pSwapChain, SyncInterval, Flags);
			g_Running = FALSE;
			return 0;

		}
		g_D3DRenderer->Overlay(pSwapChain);
		return g_D3DRenderer->oPresent(pSwapChain, SyncInterval, Flags);

    }

	void D3DRenderer::HookExecuteCommandLists(ID3D12CommandQueue* queue, UINT NumCommandLists, ID3D12CommandList* ppCommandLists) 
	{
		if (!g_D3DRenderer->m_CommandQueue)
			g_D3DRenderer->m_CommandQueue = queue;

		g_D3DRenderer->oExecuteCommandLists(queue, NumCommandLists, ppCommandLists);
	}

	bool D3DRenderer::Init(IDXGISwapChain3* swapChain)
	{
		m_Swapchain = swapChain;
		if (SUCCEEDED(m_Swapchain->GetDevice(__uuidof(ID3D12Device), (void**)&m_Device))) {
			ImGui::CreateContext();

			ImGuiIO& io = ImGui::GetIO(); (void)io;
			ImGui::GetIO().WantCaptureMouse || ImGui::GetIO().WantTextInput || ImGui::GetIO().WantCaptureKeyboard;
			io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
			io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;
			io.IniFilename = NULL;

			DXGI_SWAP_CHAIN_DESC Desc;
			m_Swapchain->GetDesc(&Desc);
			Desc.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
			Desc.OutputWindow = programData->m_GameWindow;
			Desc.Windowed = ((GetWindowLongPtr(programData->m_GameWindow, GWL_STYLE) & WS_POPUP) != 0) ? false : true;

			m_BuffersCounts = Desc.BufferCount;
			m_FrameContext = new _FrameContext[m_BuffersCounts];

			D3D12_DESCRIPTOR_HEAP_DESC DescriptorImGuiRender = {};
			DescriptorImGuiRender.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
			DescriptorImGuiRender.NumDescriptors = m_BuffersCounts;
			DescriptorImGuiRender.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;

			if (m_Device->CreateDescriptorHeap(&DescriptorImGuiRender, IID_PPV_ARGS(&m_DescriptorHeap)) != S_OK)
				return 0;

			ID3D12CommandAllocator* Allocator;
			if (m_Device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&Allocator)) != S_OK)
				return 0;

			for (size_t i = 0; i < m_BuffersCounts; i++) {
				m_FrameContext[i].CommandAllocator = Allocator;
			}

			if (m_Device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, Allocator, NULL, IID_PPV_ARGS(&m_CommandList)) != S_OK || m_CommandList->Close() != S_OK)
				return 0;

			D3D12_DESCRIPTOR_HEAP_DESC DescriptorBackBuffers;
			DescriptorBackBuffers.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
			DescriptorBackBuffers.NumDescriptors = m_BuffersCounts;
			DescriptorBackBuffers.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
			DescriptorBackBuffers.NodeMask = 1;

			if (m_Device->CreateDescriptorHeap(&DescriptorBackBuffers, IID_PPV_ARGS(&m_rtvDescriptorHeap)) != S_OK)
				return 0;

			const auto RTVDescriptorSize = m_Device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
			D3D12_CPU_DESCRIPTOR_HANDLE RTVHandle = m_rtvDescriptorHeap->GetCPUDescriptorHandleForHeapStart();

			for (size_t i = 0; i < m_BuffersCounts; i++) {
				ID3D12Resource* pBackBuffer = nullptr;
				m_FrameContext[i].DescriptorHandle = RTVHandle;
				m_Swapchain->GetBuffer(i, IID_PPV_ARGS(&pBackBuffer));
				m_Device->CreateRenderTargetView(pBackBuffer, nullptr, RTVHandle);
				m_FrameContext[i].Resource = pBackBuffer;
				RTVHandle.ptr += RTVDescriptorSize;
			}

			programData->m_GameWindow = FindWindowEx(NULL, NULL, "ELDEN RING�", NULL);
			if (programData->m_GameWindow == NULL)
			{
				Logger::log("Failed to find \"ELDEN RING�\" window, trying to get via GetForegroundWindow", LogLevel::Warning);
				programData->m_GameWindow = GetForegroundWindow();
			}

			m_OldWndProc = SetWindowLongPtrA(programData->m_GameWindow, GWLP_WNDPROC, (LONG_PTR)WndProc);

			ImGui_ImplWin32_Init(programData->m_GameWindow);
			ImGui_ImplDX12_Init(m_Device, m_BuffersCounts, DXGI_FORMAT_R8G8B8A8_UNORM, NULL, m_DescriptorHeap->GetCPUDescriptorHandleForHeapStart(), m_DescriptorHeap->GetGPUDescriptorHandleForHeapStart());
			ImGui_ImplDX12_CreateDeviceObjects();
			ImGui::GetIO().ImeWindowHandle = programData->m_GameWindow;
			m_Device->Release();
            return 1;
		}

        return 0;

	}

	bool D3DRenderer::InitWindow() 
	{

		WindowClass.cbSize = sizeof(WNDCLASSEX);
		WindowClass.style = CS_HREDRAW | CS_VREDRAW;
		WindowClass.lpfnWndProc = DefWindowProc;
		WindowClass.cbClsExtra = 0;
		WindowClass.cbWndExtra = 0;
		WindowClass.hInstance = GetModuleHandle(NULL);
		WindowClass.hIcon = NULL;
		WindowClass.hCursor = NULL;
		WindowClass.hbrBackground = NULL;
		WindowClass.lpszMenuName = NULL;
		WindowClass.lpszClassName = "MJ";
		WindowClass.hIconSm = NULL;
		RegisterClassEx(&WindowClass);
		WindowHwnd = CreateWindow(WindowClass.lpszClassName, "DirectX Window", WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_THICKFRAME, 0, 0, 100, 100, NULL, NULL, WindowClass.hInstance, NULL);
		if (WindowHwnd == NULL)
			return false;

		return true;
	}

	void D3DRenderer::ResetRenderState()
	{
		if (m_Init)
		{
			m_DescriptorHeap->Release();
			for (UINT i = 0; i < m_BuffersCounts; ++i)
			{
				m_CommandAllocator[i]->Release();
				m_BackBuffer[i]->Release();
			}
			m_rtvDescriptorHeap->Release();
			delete[]m_CommandAllocator;
			delete[]m_BackBuffer;

			ImGui_ImplDX12_Shutdown();
			//Windows_Hook::Inst()->resetRenderState();
			SetWindowLongPtr(programData->m_GameWindow, GWLP_WNDPROC, (LONG_PTR)m_OldWndProc);
			ImGui_ImplWin32_Shutdown();
			ImGui::DestroyContext();

			m_Init = false;
		}
	}

	bool D3DRenderer::DeleteWindow() 
	{
		DestroyWindow(WindowHwnd);
		UnregisterClass(WindowClass.lpszClassName, WindowClass.hInstance);
		if (WindowHwnd != NULL) {
			return false;
		}
		return true;
	}

    bool D3DRenderer::CreateHook(uint16_t Index, void** Original, void* Function) 
	{
        void* target = (void*)MethodsTable[Index];
        if (MH_CreateHook(target, Function, Original) != MH_OK || MH_EnableHook(target) != MH_OK) {
            return false;
        }
        return true;
    }

    void D3DRenderer::DisableHook(uint16_t Index) 
	{
        MH_DisableHook((void*)MethodsTable[Index]);
    }

    void D3DRenderer::DisableAll() 
	{
		DisableHook(MethodsTable[54]);
		DisableHook(MethodsTable[140]);
		DisableHook(MethodsTable[146]);
    }

	D3DRenderer::~D3DRenderer() noexcept
	{
		g_Running = false;
		Unhook();
	}
}