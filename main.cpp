/*/INCLUDES/*/
#include <windows.h>				// for Windows API 
#include <d3d12.h>					// for Direct3D 12  
#include <dxgi1_6.h>                // for DXGI usage
#include "d3dx12.h"					// for Direct3D 12 helpers
#include <d3dcompiler.h>            // for Direct3D 12 shader compilation
#include <DirectXMath.h>			// for Direct3D 12 math
#include <wrl.h>					// for COM objects
#include <iostream>					// for input and output 
#include <memory>					// for modern memory management in C++
#include <cstdlib>					// for parts that may need malloc and free
#include <synchapi.h>				// for some parts of Direct3D 12 
#include <string>					// for strings
#include <stdexcept>				// for exceptions





/*/LINKER/*/
#pragma comment(lib,"d3d12.lib")       // linking Direct3D 12 library
#pragma comment(lib,"dxgi.lib")        // linking DXGI for setting up Direct3D 12
#pragma comment(lib,"d3dcompiler.lib") // linking the shader compiler for Direct3D 12


/*/CONSTANTS/*/
const uint32_t SCR_WIDTH = 800;
const uint32_t SCR_HEIGHT = 600;
const wchar_t* SCR_TITLE = L"D3D12 WINDOW";
const wchar_t* CLASS_NAME = L"D3D12";
/*/RENDERER/*/
class Renderer
{


public:
	//the constructor
	Renderer()
	{
		//initializing the values with NULL
		wc = {};
		hWnd = {};
		msg = {};
	}

	//the destructor
	~Renderer()
	{

	}

	//the main run function
	void run(HINSTANCE hInstance,int nCmdshow)
	{
#ifndef NDEBUG
		initConsole();
#endif 
		initWindow(hInstance, nCmdshow);

		initD3D12();

		mainLoop();

		cleanup();
	}


private:
	//window class
	WNDCLASSEX wc;
	//window handler
	HWND hWnd;
	//msg for translation and dispatching with wndProc function
	MSG msg;
	//this is the D3D12 part. get ready...
	//swap chain is a concept that modern graphics API use a lot.
	//it's the process of changing the address of buffers in order with each other for fast iteration of frames and buffers
	//when beginning it's better to use 2 frames. it's called double buffering.
	//hence our frameCount is 2 now...
	static const UINT frameCount = 2;
	//swap chain object. swap chain exactly manages swapping the back and front buffers in a chain for us!
	Microsoft::WRL::ComPtr<IDXGISwapChain3> swapChain;
	//device object
	Microsoft::WRL::ComPtr<ID3D12Device> device;

	//render targets. these are the targeted frames or buffer i shall say that we will render stuff to
	Microsoft::WRL::ComPtr<ID3D12Resource> renderTargets[frameCount];
	//command allocator object. it manages the allocation for recording commands into command list
	Microsoft::WRL::ComPtr<ID3D12CommandAllocator> commandAllocator;
	//command list object. it's the object that manages the instructs or shall say commands to GPU.
	Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> commandList;
	//command queue object. it manages the execution of our command lists...
	//i think now you realize why the order of command* objects are like this in my code
	Microsoft::WRL::ComPtr<ID3D12CommandQueue> commandQueue;
	//root signature object. it defines how shader accesses resources during pipeline execution
	Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature;
	//render target view descriptor heap. descriptor heaps create an array of descriptors.
	//our type is render target view now because we use it for our render targets.
	//descriptors are the things that manage how GPU accesses textures,buffers and overall resources...
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> rtvHeap;
	//pipeline state. we config our renderer with it. configuring the graphics pipeline.
	Microsoft::WRL::ComPtr<ID3D12PipelineState> pipelineState;
	//the size for our render target view descriptor heap.
	UINT rtvDescriptorSize;
	//our vertex buffer. we store vertex in it and pass it to our shaders.
	//as you might have noticed it's exactly defined how to get used by our descriptor heap.
	Microsoft::WRL::ComPtr<ID3D12Resource> vertexBuffer;
	//vertexBufferView is an object that helps us make GPU interpret our vertex data we defined by vertex struct and passed to vertex buffer
	D3D12_VERTEX_BUFFER_VIEW vertexBufferView;
	//manages the current index of our frame count for when we manage the render targets and other stuff for each frame
	UINT frameIndex;
	//fence is a cool concept. fence basically checks if GPU finished it's job or not to the command queue. so stuff can be executed correctly 
	Microsoft::WRL::ComPtr<ID3D12Fence> fence;
	//fence event checks the value or signal of fence stored in fenceValue.
	//and if it's good it will tell the CPU that GPU done it's thing
	HANDLE fenceEvent;
	//this is the value we get from the fence.get() and we store it
	UINT64 fenceValue;
	//setting the aspect ratio
	float aspectRatio = static_cast<float>(SCR_WIDTH) / static_cast<float>(SCR_HEIGHT);
	//setting the viewport and scissor rect
	D3D12_VIEWPORT viewPort;
	D3D12_RECT scissorRect;
	//our vertex struct. it currently receives positions and colors. but you can extend it to UV and normal maps.
	struct Vertex
	{
		DirectX::XMFLOAT3 pos;
		DirectX::XMFLOAT4 color;
	};
	// the callback wndProc. it's purpose is to communicate with our window usually and give messages to it via our message loop
	static LRESULT CALLBACK wndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
	{
		switch (uMsg)
		{
		case WM_DESTROY:
			PostQuitMessage(0);
			return 0;
		default:
			return DefWindowProc(hWnd, uMsg, wParam, lParam);
		}
	}








	//a helper function for managing HRESULTS across our stuff
	//you might ask what is an HRESULT?
	//well an HRESULT is usually what most Microsoft's WINAPI and DX ecosystem functions give.
	//which says if something is done correctly or not or other kind of results
	//if you are coming from Vulkan i have a secret for ya... it's VkResult!
	void throwIfFailed(HRESULT hr)
	{
		if (FAILED(hr))
		{
#ifndef NDEBUG
			//if in debug mode you will receive a debug message!
			std::cerr << "HRESULT failed: 0x" << std::hex << hr << std::endl;
#endif
			throw std::exception();
		}
	}









// i don't like explaining this but i have to...
// whenever you see ifndef NDEBUG just know this code only works in debug mode...
#ifndef NDEBUG
	//makes the debug console
	void initConsole()
	{
		//allocation of the debug console
		AllocConsole();
		//setting the title of the console
		SetConsoleTitle(L"DIRECT 3D DEBUG CONSOLE");
		//redirecting std::cout and std::cerr to the console
		FILE* pFile;
		freopen_s(&pFile, "CONOUT$", "w", stdout);
		FILE* pErr;
		freopen_s(&pErr, "CONOUT$", "w", stderr); 
		//for syncing the streams and making it clean
		std::ios::sync_with_stdio(); 
		std::wcout.clear(); 
		//making a handle for console
		HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
		//setting console attributes. RED | BLUE | INTENSITY => INTENSE PURPLE
		SetConsoleTextAttribute(hConsole, FOREGROUND_RED | FOREGROUND_BLUE | FOREGROUND_INTENSITY);
		// a message to make us happy
		std::cout << "Console initialized successfully!" << std::endl;
	
	}

#endif












	//initializes a window
	void initWindow(HINSTANCE hInstance,int nCmdShow)
	{
		//setting the size of the window class struct
		wc.cbSize = sizeof(WNDCLASSEX);
		//setting the style of it to CS_OWNDC. just know it makes the redraw tied to our Direct 3D device
		wc.style = CS_OWNDC;
		//giving it our wndProc so it can send messages to it
		wc.lpfnWndProc = wndProc;
		wc.cbClsExtra = 0;
		wc.cbWndExtra = 0;
		//giving it out instance
		wc.hInstance = hInstance;
		//loading default icon and cursor.
		//if you want to make your own stuff for it later...
		//you need to create resource files and generate an header to interact with them
		wc.hIcon = LoadIcon(NULL, IDI_APPLICATION);
		wc.hIconSm = LoadIcon(NULL, IDI_APPLICATION);
		wc.hCursor = LoadCursor(NULL, IDC_ARROW);
		//setting a placeholder background color till we give the device our window handler
		wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
		//it has no menu so we set it to NULL
		wc.lpszMenuName = NULL;
		//giving it the class name we defined in our CONSTANTS
		wc.lpszClassName = CLASS_NAME;


		//registering the window class
		RegisterClassEx(&wc);


		//we are creating the window and it's handler now with this
		hWnd = CreateWindowEx(
			0,								//for extended styles that we don't need now. but for you curious basterd i say check out WS_EX_* stuff
			CLASS_NAME,						//the class name we had in our CONSTANTS and gave to our wc
			SCR_TITLE,                      //the title we defined for our window
			WS_OVERLAPPEDWINDOW,                  //normal styles. it's a normal title bar having window
			CW_USEDEFAULT, CW_USEDEFAULT,   //default starting placement position for window
			SCR_WIDTH, SCR_HEIGHT,          //the starting window width and height
			NULL,                           //this is for when your window is a child. you need to pass the parent window hWnd to this.
			NULL,                           //this is for the menu. we don't have no menu.
			hInstance,                      //passing our instance
			NULL                            //we don't have any extra data we need give to wndProc and . so NULL again.
		);
		//this is the hack type voodoo we have here.
		//it's purpose is only to give us arbitrary ptr when we need to access class members in a static function
		//via the reinterpret cast sorry not sorry.
		SetWindowLongPtr(hWnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(this));


		//the name says it all. show me the window!!!
		ShowWindow(hWnd, nCmdShow);
		//updates the hWnd
		UpdateWindow(hWnd);


	}





	

	void initD3D12()
	{
		loadPipeline();

		loadAssets();
	}

	









	void loadPipeline()
	{
#ifndef NDEBUG
		{
			//enables D3D12 debug layer in debug mode.
			Microsoft::WRL::ComPtr<ID3D12Debug> debugController;
			//it checks if it can get the debugController interface and enabled the debug layer for it.
			//thought it's better to explain a bit of COM architecture to you.
			//each COM object has it's UUID for it's interface.
			//you gonna see tons of IID_PPV_ARGS in this code.
			//what IID_PPV_ARGS does is that it gets the Interface of the COM ID
			//hence the part of the name IID(Interface ID)
			//it's a macro that wraps the interface ID and pointer.
			if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController))))
			{
				debugController->EnableDebugLayer();
			}
		}
#endif 
		//factory object. it helps to make enumerate adapters(you might like to call it GPU or CPU software rendering) and create swap chain.
		//thought to explain another thing to ya about ComPtr naming conventions.
		//Microsoft::WRL::ComPtr<I<OBJECT_NAME><VERSION>>.
		//by IDXGIFactory4 we mean interface of DXGIFactory object version 4.
		//each version adds new capabilities  v4 supports features like adapter enumeration with flags.
		//i think you remember IDXGISwapChain3 now xD. same naming logic.
		Microsoft::WRL::ComPtr<IDXGIFactory4> factory;
		
		//from now on you shall know that we care about the side effect of these commands
		//not what they return. which is the HRESULT.
		//these commands try to do some stuff as side effects.
		//like creating a factory here and passing it to our factory object.
		//if it fails we will throw out via our helper function.
		throwIfFailed(CreateDXGIFactory1(IID_PPV_ARGS(&factory)));

		// MSDN goes through a warp device creation process here. but i won't.
		// it's for software rendering...
		// WARP is a software rasterizer (runs on CPU), useful for fallback or testing.
		// honestly if you gonna make a software renderer stick to dx9
		// because most of stuff happen on CPU there not on shaders which are perfect for GPUS.


		Microsoft::WRL::ComPtr<IDXGIAdapter1> hardwareAdapter;
		
		//this is a simple funtion that checks which GPU of your system supports D3D12.
		//actually it checks which GPU of your system has feature level 11 of D3D12.
		//i will explain it to you more later...
		//for now this sets the hardware adapter from your adapters
		for (UINT adapterIndex = 0; ; ++adapterIndex)
		{
			Microsoft::WRL::ComPtr<IDXGIAdapter1> adapter;
			if (DXGI_ERROR_NOT_FOUND == factory->EnumAdapters1(adapterIndex, &adapter))
			{
				break;
			}
			DXGI_ADAPTER_DESC1 desc;
			adapter->GetDesc1(&desc);
			if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE)
			{
				continue;
			}
			// Check if adapter supports D3D12
			if (SUCCEEDED(D3D12CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_11_0, __uuidof(ID3D12Device), nullptr)))
			{
				hardwareAdapter = adapter;
				break;
			}
			//tells you what adapter you are using
			std::wcout << L"Using adapter: " << desc.Description << std::endl;

		}
		//it tries to create a D3D12 device with your hardware adapter and feature level we set.
		//even though the api is D3D12 we are currently writing with D3D11 feature levels.
		//this is useful when you want multithread and control goodness of D3D12 but also want to support older hardware with feature level of D3D 11.
		throwIfFailed(D3D12CreateDevice(hardwareAdapter.Get(), D3D_FEATURE_LEVEL_11_0,IID_PPV_ARGS(&device)));


		//describing and creating a command queue.
		D3D12_COMMAND_QUEUE_DESC queueDesc = {};
		queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;	//the flag is none here. cuz GPU isn't doing anything intense which will cuz it to timeout.
		queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;    //setups the command queue to command the GPU directly.

		//creates the Command queue from the description we made for it and passes it to our interface of commandQueue COM Object.
		throwIfFailed(device->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&commandQueue)));


		//gotta make that swapchain xD
		DXGI_SWAP_CHAIN_DESC swapChainDesc = {};
		//making the back buffer counts as the frameCount we set before.
		swapChainDesc.BufferCount = frameCount;
		//setting the width and height of the swap chain to constants we created before.
		swapChainDesc.BufferDesc.Width = SCR_WIDTH;
		swapChainDesc.BufferDesc.Height = SCR_HEIGHT;
		//setting the format of our swapchain to 32bit colors. nice...
		swapChainDesc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		//the buffer is used to render target.
		swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
		//the effect that is done while swapping.it swaps and discards the back buffer so you don't see any artifacts.
		swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
		//passing the handle of window to it so we can render stuff on the window
		swapChainDesc.OutputWindow = hWnd;
		//this is for msaa anti analyzing. we don't care about it. so sample count is 1 and quality is 0.
		swapChainDesc.SampleDesc.Count = 1;
		swapChainDesc.SampleDesc.Quality = 0;
		//our demo is windowed.
		swapChainDesc.Windowed = TRUE;
		//we make a initial swap chain and pass the description to it for creating it.
		Microsoft::WRL::ComPtr<IDXGISwapChain> initialSwapChain;
		throwIfFailed(factory->CreateSwapChain(commandQueue.Get(), &swapChainDesc, &initialSwapChain));
		//we set the initial swap chain as the main swap chain.
		//the reason we are having a initial swap chain and the main swap chain is that creating the swap chain function works with the base one
		//and using the multibuffer and getting the current backbuffer needs the 3rd version
		throwIfFailed(initialSwapChain.As(&swapChain));
		//this demo doesn't support fullscreening. so we disable alt_enter for it so you can't fullscreen.
		throwIfFailed(factory->MakeWindowAssociation(hWnd, DXGI_MWA_NO_ALT_ENTER));
		//getting the current back buffer index from the swap chain.
		//this tells us which render target to use  it cycles between 0 and 1 in double buffering
		frameIndex = swapChain->GetCurrentBackBufferIndex();

		//descriptor heap creation. i told you what it does.
		{
			//descriptor heap description
			D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc = {};
			//as i told you before we have descriptors for every frame count
			rtvHeapDesc.NumDescriptors = frameCount;
			//the type of are descriptor is render target view hence the type RTV. cuz we render target views with it.
			rtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
			//there is no flags needed for this so NONE.
			rtvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
			//we create the descriptor heap with the description
			throwIfFailed(device->CreateDescriptorHeap(&rtvHeapDesc, IID_PPV_ARGS(&rtvHeap)));
			//we set the size of the descriptor heap with getting it's size from the type
			rtvDescriptorSize = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
		}
		//creating frame resources
		{
			//time to use this D3DX12.h lib we got here XDDDD
			//the only helper header i will use cuz no one can live without it in D3D12
			//we get the start of rtv descriptor heap from this and pass it to the handle
			CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(rtvHeap->GetCPUDescriptorHandleForHeapStart());
			//a loop that creates resources for render targts and creates render target views in general
			for (UINT n = 0; n < frameCount; n++)
			{
				throwIfFailed(swapChain->GetBuffer(n, IID_PPV_ARGS(&renderTargets[n])));
				device->CreateRenderTargetView(renderTargets[n].Get(), nullptr, rtvHandle);
				rtvHandle.Offset(1, rtvDescriptorSize);
			}
		}
		{
			//initializing the viewport and scissor rect
			//treat them as the stuff that d3d12 pipeline gets for managing the resize and size of the window and rendering the target based on it
			//usually stuff get rebuilt with new information of window size every time you resize.
			//but this example doesn't have any resize so we don't care
			viewPort = { 0.0f, 0.0f, static_cast<float>(SCR_WIDTH), static_cast<float>(SCR_HEIGHT), 0.0f, 1.0f };
			scissorRect = { 0, 0, static_cast<LONG>(SCR_WIDTH), static_cast<LONG>(SCR_HEIGHT) };
		}
		//we allocate heap for the command list here that will run with command queues under swap chain.
		throwIfFailed(device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&commandAllocator)));
	}












	void loadAssets()
	{
		//creating an empty root signature

		CD3DX12_ROOT_SIGNATURE_DESC rootSignatureDesc;
		//setting the flag that allows input assembler stage do it's thing for inputs
		rootSignatureDesc.Init(0, nullptr, 0, nullptr, D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);
		Microsoft::WRL::ComPtr<ID3DBlob> signature;
		Microsoft::WRL::ComPtr<ID3DBlob> error;
		//serializing and creating root signature
		throwIfFailed(D3D12SerializeRootSignature(&rootSignatureDesc, D3D_ROOT_SIGNATURE_VERSION_1, &signature, &error));
		throwIfFailed(device->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(), IID_PPV_ARGS(&rootSignature)));



		//creating vertex shader and pixel shader
		Microsoft::WRL::ComPtr<ID3DBlob> vertexShader;
		Microsoft::WRL::ComPtr<ID3DBlob> pixelShader;

		//depending on debug mode or not setting the compiler flags to be there or not.
#ifndef NDEBUG
		UINT compileFlags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#else 
		UINT compileFlags = 0;
#endif 
		//compiling the shaders
		throwIfFailed(D3DCompileFromFile(L"shaders.hlsl", nullptr, nullptr, "VSMain", "vs_5_0", compileFlags, 0, &vertexShader, nullptr));
		throwIfFailed(D3DCompileFromFile(L"shaders.hlsl", nullptr, nullptr, "PSMain", "ps_5_0", compileFlags, 0, &pixelShader, nullptr));
		//setting how input elements are interpreted to the shader
		D3D12_INPUT_ELEMENT_DESC inputElementDescs[] =
		{
			{"POSITION",0,DXGI_FORMAT_R32G32B32_FLOAT,0,0,D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,0},
			{"COLOR",0,DXGI_FORMAT_R32G32B32A32_FLOAT,0,12,D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,0}
		};
		{
		//setting the pipeline state
		D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
		//giving it our input layout we created
		psoDesc.InputLayout = { inputElementDescs,_countof(inputElementDescs) };
		//giving it the root signature
		psoDesc.pRootSignature = rootSignature.Get();
		//giving it our vertex and pixel shader
		psoDesc.VS = { reinterpret_cast<UINT8*>(vertexShader->GetBufferPointer()),vertexShader->GetBufferSize() };
		psoDesc.PS = { reinterpret_cast<UINT8*>(pixelShader->GetBufferPointer()),pixelShader->GetBufferSize() };
		//setting the rasterizer as the default one
		psoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
		//and also the blending state 
		psoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
		//disabling depth and stencil buffer for now
		psoDesc.DepthStencilState.DepthEnable = FALSE;
		psoDesc.DepthStencilState.StencilEnable = FALSE;
		psoDesc.SampleMask = UINT_MAX;
		//setting the render type to render in triangles for our pipeline
		psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
		psoDesc.NumRenderTargets = 1;
		//the format for our render target. we set this before in swapchain too
		psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
		//remember multisampling stuff i said before? they are here too
		psoDesc.SampleDesc.Count = 1;
		psoDesc.SampleDesc.Quality = 0;
		//creating the pipeline
		throwIfFailed(device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&pipelineState)));
		}
		//creating the command list with our information
		throwIfFailed(device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, commandAllocator.Get(), pipelineState.Get(), IID_PPV_ARGS(&commandList)));
		//and closing it for later that we do stuff with it
		throwIfFailed(commandList->Close());

		{
			//beautiful triangle vertices
			Vertex triangleVertices[] =
			{
				// Top vertex bright violet
				{ { 0.0f, 0.25f * aspectRatio, 0.0f }, { 0.8f, 0.0f, 0.8f, 1.0f } },

				// Bottom right deep blue
				{ { 0.25f, -0.25f * aspectRatio, 0.0f }, { 0.2f, 0.0f, 0.6f, 1.0f } },

				// Bottom left rich red
				{ { -0.25f, -0.25f * aspectRatio, 0.0f }, { 0.6f, 0.0f, 0.2f, 1.0f } }
			};
			//setting the size of our vertex buffer
			const UINT vertexBufferSize = sizeof(triangleVertices);
			//this type isn't efficient as MSDN notes cuz it has tons of overhead. but a triangle is simple eh?
			//we are setting how descriptor heap uploads stuff
			CD3DX12_HEAP_PROPERTIES heapProps(D3D12_HEAP_TYPE_UPLOAD);
			//setting resource description for our vertex buffer
			auto desc = CD3DX12_RESOURCE_DESC::Buffer(vertexBufferSize);
			throwIfFailed(device->CreateCommittedResource(&heapProps, D3D12_HEAP_FLAG_NONE, &desc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&vertexBuffer)));
			//a pointer to where vertex data even begins
			UINT8* pVertexDataBegin;
			//making a renge
			CD3DX12_RANGE readRange(0, 0);
			//mapping that range
			throwIfFailed(vertexBuffer->Map(0, &readRange, reinterpret_cast<void**>(&pVertexDataBegin)));
			//copying the triangle verteices to our array of vertex data. or vertex data pointer i shall say.
			//array is just the pointer to the beginning of an arranged memory anyway XDD
			memcpy(pVertexDataBegin, triangleVertices, sizeof(triangleVertices));
			//unmapping the vertex buffer
			vertexBuffer->Unmap(0, nullptr);

			// Initialize the vertex buffer view.
			vertexBufferView.BufferLocation = vertexBuffer->GetGPUVirtualAddress();
			//setting our strides. as we know we have an array of Vertex structs.
			//so stride for our data has the same size as it.
			vertexBufferView.StrideInBytes = sizeof(Vertex);
			//we got the size before from Vertex struct sizeof
			vertexBufferView.SizeInBytes = vertexBufferSize;
		}
		{
			//creating fence for handling CPU-GPU scheduling
			throwIfFailed(device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence)));
			//the initial value is 1
			fenceValue = 1;
			//creating a fence event that checks and signals if gpu is done or not with the cpu
			fenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
			if (fenceEvent == nullptr)
			{
				//error if it wasn't created
				throwIfFailed(HRESULT_FROM_WIN32(GetLastError()));
			}
			//waiting for the previous frame
			//it's just here to start up the fence management stuff honestly.
			waitForPreviousFrame();

		}
	}








	void onRender()
	{
		//populating command lists
		populateCommandList();
		//making an array of our command lists and giving it to command queue
		ID3D12CommandList* ppCommandList[] = { commandList.Get() };
		commandQueue->ExecuteCommandLists(_countof(ppCommandList), ppCommandList);
		//presenting buffers
		throwIfFailed(swapChain->Present(1, 0));
		//waiting for previous frame
		waitForPreviousFrame();
	}








	void onUpdate()
	{
		//empty for this program
	}









	void waitForPreviousFrame()
	{
		//waiting for previous frame always isn't a good thing for every frame
		//as msdn said it has lots of overhead and it might makes the scenes not render very fast and all
		//but we use it here
		//sFence is basically the second fence value..or shall say curremt femce value... it's the value i set for comparings and the fence value is inceremented  every time we wait for a frame
		const UINT64 sFence = fenceValue;
		throwIfFailed(commandQueue->Signal(fence.Get(), sFence));
		fenceValue++;

		// Wait until the previous frame is finished.
		if (fence->GetCompletedValue() < sFence)
		{
			throwIfFailed(fence->SetEventOnCompletion(sFence, fenceEvent));
			WaitForSingleObject(fenceEvent, INFINITE);
		}
		//getting the current back buffer index
		frameIndex = swapChain->GetCurrentBackBufferIndex();
	}













	void populateCommandList()
	{
		//resetting the command allocator and command list for this frame
		throwIfFailed(commandAllocator->Reset());
		throwIfFailed(commandList->Reset(commandAllocator.Get(), pipelineState.Get()));
		//setting the execute commands.
		//we set the view port and root signature and scissorRect for our program
		commandList->SetGraphicsRootSignature(rootSignature.Get());
		commandList->RSSetViewports(1, &viewPort);
		commandList->RSSetScissorRects(1, &scissorRect);
		// Transition the back buffer from present to render target state
		auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(renderTargets[frameIndex].Get(), D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET);
		commandList->ResourceBarrier(1, &barrier);
		//a handler for our descriptor heap for the current back buffer
		CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(rtvHeap->GetCPUDescriptorHandleForHeapStart(), frameIndex, rtvDescriptorSize);
		//setting render targets
		commandList->OMSetRenderTargets(1, &rtvHandle, FALSE, nullptr);

		//record commands.
		//clearing the screen with a sexy near black color
		const float clearColor[] = { 0.1f, 0.1f, 0.1f, 1.0f };
		//passing the clearColor array the command list
		commandList->ClearRenderTargetView(rtvHandle, clearColor, 0, nullptr);
		//making the GPU render with triangles..not with lines or points
		commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		//setting the vertex buffer for our scene. that vertex buffer we created before.
		commandList->IASetVertexBuffers(0, 1, &vertexBufferView);
		//and finally drawing from our precious vertex buffer
		//3 vertices. 1 instance
		commandList->DrawInstanced(3, 1, 0, 0);

		//transition the back buffer from render target to present state
		barrier = CD3DX12_RESOURCE_BARRIER::Transition(renderTargets[frameIndex].Get(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT);
		commandList->ResourceBarrier(1, &barrier);
		//closing the command list for execution. packaged and ready to send with command queue
		throwIfFailed(commandList->Close());

	}






	void onDestroy()
	{
		// Wait for the GPU to be done with all resources.
		waitForPreviousFrame();

		CloseHandle(fenceEvent);
	}








	// the main loop
	void mainLoop()
	{
		//runs the loop till we quit
		while (msg.message != WM_QUIT)
		{
			//translates and dispatches messages to wndProc whenever there's a new message
			//Peek is the best for Real time Rendering because we really care about updates even when nothing happens
			while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE))
			{
				TranslateMessage(&msg);
				DispatchMessage(&msg);
			}
			//this is the part you put your stuff in for drawing stuff....
			onRender();
			onUpdate();
		}
	}










	//the cleanup function
	void cleanup()
	{
		//destroying our D3D12 instance
		onDestroy();
#ifndef NDEBUG
		//closes the console  if you are in debug mode
		FreeConsole();
#endif 
	}
};











//beautiful main function of the glorious WINAPI
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdline, int nCmdShow)
{
	//tries to run the renderer class
	try
	{
		// what can i say. i love std::make_unique'ing all over the place
		// it just runs the renderer anonymously
		std::make_unique<Renderer>()->run(hInstance, nCmdShow);
	}
	//for case of failure
	catch (const std::exception& e)
	{
		//gives you the exception error.
		std::cerr << "ERROR: " << e.what() << std::endl;
		return EXIT_FAILURE;
	}
	return EXIT_SUCCESS;
}
