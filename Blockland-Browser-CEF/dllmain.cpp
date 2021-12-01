#define ConsoleMethod(type, function) type ts_##function(ADDR obj, int argc, const char* argv[])
#define RegisterMethod(name, description, argmin, argmax) tsf_AddConsoleFunc(NULL, NULL, #name, ts_##name, description, argmin, argmax);
#define BBR BlockBrowserRender
#define BLB_BuildNumber 1
#include <Windows.h>
#include "RedoBlHooks.hpp"
#include "torque.hpp"
#include <string>
#include <map>
#include <psapi.h>
#include <shlwapi.h>

#include <include/base/cef_bind.h>
#include <include/cef_app.h>
#include <include/cef_parser.h>
#include <include/wrapper/cef_helpers.h>
#include <include/cef_base.h>

#include <include/cef_browser.h>
#include <include/cef_client.h>

#include <include/wrapper/cef_closure_task.h>
#include "glapi.h"
#pragma comment(lib, "Shlwapi.lib")
#pragma comment(lib, "libcef.lib")
#pragma comment(lib, "libcef_dll_wrapper.lib")

#include <thread>

HANDLE BlockBrowserThread;
bool* doBreakPtr = new bool(true);
char* texBuffer;
static bool* isDirty = new bool(false);
int* globalTextureID = new int(-1);
TextureObject** smTable;
CefRefPtr<CefBrowser> browser;
#define BLBPrintf(...) \
	rbh_BlPrintf("BLBrowser: " __VA_ARGS__);

class BlockBrowser : public CefApp {
public:
	BlockBrowser() {};
private:
	IMPLEMENT_REFCOUNTING(BlockBrowser);
};
class BBR : public CefRenderHandler {
public:
	BBR(int w, int h) : width(h), height(h) {

	}
	~BBR() {

	}
	virtual CefRefPtr<CefAccessibilityHandler> GetAccessibilityHandler() { return nullptr; }

	virtual bool GetRootScreenRect(CefRefPtr<CefBrowser> browser, CefRect& rect) {
		return false;
	}
	virtual bool GetScreenInfo(CefRefPtr<CefBrowser> browser, CefScreenInfo& info) {
		info.rect = CefRect(0, 0, width, height);
		info.device_scale_factor = 1.0;
		return true;
	}
	virtual bool GetScreenPoint(CefRefPtr<CefBrowser> browser, int vx, int vy, int& sx, int& sy) {
		return false;
	}
	virtual void GetViewRect(CefRefPtr<CefBrowser> browser, CefRect& rect) {
		CEF_REQUIRE_UI_THREAD();
		rect = CefRect(0, 0, width, height);
	}
	virtual void OnAcceleratedPaint(CefRefPtr<CefBrowser> browser, CefRenderHandler::PaintElementType type, const CefRenderHandler::RectList& dirtyRects, void* shared_handle) { }
	virtual void OnImeCompositionRangeChanged(CefRefPtr< CefBrowser > browser, const CefRange& selected_range, const CefRenderHandler::RectList& character_bounds) { }
	virtual void OnPaint(CefRefPtr< CefBrowser > browser, CefRenderHandler::PaintElementType type, const CefRenderHandler::RectList& dirtyRects, const void* buffer, int w, int h) {
		CEF_REQUIRE_UI_THREAD()
			if (*globalTextureID != 0) {
				for (CefRect rect : dirtyRects) {
					for (int i = 0; i < rect.height; i++) {
						int offset = rect.x * 4 + (rect.y+i) * h * 4;
						memcpy((void*)(((unsigned int)texBuffer) + offset), (const void*)(offset + ((unsigned int)buffer)), rect.width * 4);
					}
				}
				*isDirty = true;
			}
	}
	virtual void OnPopupShow(CefRefPtr< CefBrowser > browser, bool show) { }
	virtual void OnPopupSize(CefRefPtr< CefBrowser > browser, const CefRect& rect) { }
	virtual void OnScrollOffsetChanged(CefRefPtr< CefBrowser > browser, double x, double y) { }
	virtual void OnTextSelectionChanged(CefRefPtr< CefBrowser > browser, const CefString& selected_text, const CefRange& selected_range) {}
	virtual void OnVirtualKeyboardRequested(CefRefPtr< CefBrowser > browser, CefRenderHandler::TextInputMode input_mode) { }
	virtual bool StartDragging(CefRefPtr< CefBrowser > browser, CefRefPtr< CefDragData > drag_data, CefRenderHandler::DragOperationsMask allowed_ops, int x, int y) { return false; }
	void UpdateDragCursor(CefRefPtr< CefBrowser > browser, CefRenderHandler::DragOperation operation) { }
	void UpdateResolution(int w, int h) {
		if (w * h * 4 > 16777216 || w * h * 4 < 262144)
			return;
		//memset(texBuffer, 0, w * h * 4);
		width = w;
		height = h;
	}
	const int getWidth() { return width; }
	const int getHeight() { return height; }
private:
	IMPLEMENT_REFCOUNTING(BlockBrowserRender);
	int width, height;
};

class BrowserClient : public CefClient, public CefLifeSpanHandler, public CefLoadHandler, public CefRequestHandler, public CefDisplayHandler {
public:
	BrowserClient(CefRenderHandler* ptr) : handler(ptr) {}
	CefRefPtr<CefLifeSpanHandler> GetLifeSpanHandler() override { return this; }
	CefRefPtr<CefLoadHandler> GetLoadHandler() override { return this; }
	CefRefPtr<CefRenderHandler> GetRenderHandler() override { return handler; }
	CefRefPtr<CefRequestHandler> GetRequestHandler() override { return this; }
	CefRefPtr<CefDisplayHandler> GetDisplayHandler() override { return this; }
	bool OnConsoleMessage(CefRefPtr<CefBrowser> browser, cef_log_severity_t level, const CefString& message, const CefString& source, int line) {
		switch (level) {
		default:
		case LOGSEVERITY_DEFAULT:
			BlPrintf("CEF: %s", message.ToString().c_str());
			return false;
		case LOGSEVERITY_DEBUG:
			BlPrintf("CEF[DEBUG]: %s", message.ToString().c_str());
			return false;
		case LOGSEVERITY_ERROR:
			BlPrintf("CEF[ERROR]: %s", message.ToString().c_str());
			return false;
		case LOGSEVERITY_FATAL:
			BlPrintf("CEF[FATAL]: %s", message.ToString().c_str());
			return false;
		case LOGSEVERITY_WARNING:
			BlPrintf("CEF[WARNING]: %s", message.ToString().c_str());
		return false;
		}
		return false;
	}
	void OnAfterCreated(CefRefPtr<CefBrowser> browser) {
		browser_id = browser->GetIdentifier();
	}
	bool DoClose(CefRefPtr<CefBrowser> browser) {
		if (browser->GetIdentifier() == browser_id) {
			closing = true;
		}
		return false;
	}
	void OnBeforeClose(CefRefPtr<CefBrowser> browser) { }
	void OnLoadEnd(CefRefPtr<CefBrowser> browser, CefRefPtr<CefFrame> frame, int httpStatusCode) {
		browser->GetHost()->Invalidate(CefBrowserHost::PaintElementType::PET_VIEW);
		loaded = true;
	}
	void OnLoadError(CefRefPtr<CefBrowser> browser, CefRefPtr<CefFrame> frame, CefLoadHandler::ErrorCode errorCode, const CefString& failedUrl, CefString& errorText) {
		browser->GetHost()->Invalidate(CefBrowserHost::PaintElementType::PET_VIEW);
		loaded = true;
	}
	void OnLoadingStateChange(CefRefPtr<CefBrowser> browser, bool isLoading, bool canGoBack, bool canGoForward) {
		browser->GetHost()->Invalidate(CefBrowserHost::PaintElementType::PET_VIEW);
	}
	void OnLoadStart(CefRefPtr<CefBrowser> browser, CefRefPtr<CefFrame> frame) { }
	bool closeAllowed() const { return closing; };
	bool isLoaded() const { return loaded; };
	bool OnBeforeBrowse(CefRefPtr<CefBrowser> browser,
		CefRefPtr<CefFrame> frame,
		CefRefPtr<CefRequest> request,
		bool user_gesture,
		bool is_redirect) {
		int c = 2;
		const char* _url = request->GetURL().ToString().c_str();
		char* url = new char[strlen(_url)];
		memcpy((void*)url, (const void*)_url, strlen(_url));
		char* argv[2];
		int sourceMask = request->GetTransitionType() & TT_SOURCE_MASK;
		argv[0] = (char*)"BLB_OnUrlRequest";
		argv[1] = url;
		if (sourceMask == TT_LINK || sourceMask == TT_EXPLICIT)
			rbh_BlCon__execute(c, (const char**)argv);
		return false;
	}
private:
	int browser_id;
	bool closing = false;
	bool loaded = false;
	CefRefPtr<CefRenderHandler> handler;
	IMPLEMENT_REFCOUNTING(BrowserClient);
};
CefRefPtr<BrowserClient> browser_client;
CefRefPtr<BBR> renderHandler;
ConsoleMethod(void, resizeWindow) {
	BLBPrintf("Currently unsupported.");
}
ConsoleMethod(void, keyboardEvent) {
	CefKeyEvent* keyEvent = new CefKeyEvent();
	char cu = argv[1][0];
	char c = cu;
	uint32 flags = atoi(argv[2]);
	
	if (cu >= 65 && cu <= 90) {
		c = cu + 32;
		flags |= EVENTFLAG_SHIFT_DOWN;
	}
	keyEvent->character = c;
	keyEvent->unmodified_character = cu;
	keyEvent->windows_key_code = VkKeyScanEx(cu, GetKeyboardLayout(0));
	keyEvent->modifiers = flags;
	keyEvent->focus_on_editable_field = true;
	keyEvent->native_key_code = VkKeyScanEx(cu, GetKeyboardLayout(0));
	keyEvent->is_system_key = false;
	keyEvent->type = KEYEVENT_CHAR;
	browser->GetHost()->SendKeyEvent(*keyEvent);
}
ConsoleMethod(void, mouseMove) {
	CefMouseEvent* event = new CefMouseEvent();

	event->x = (int)round(atof(argv[1]) * renderHandler->getWidth());
	event->y = (int)round(atof(argv[2]) * renderHandler->getHeight());
	browser->GetHost()->SendMouseMoveEvent(*event, false);
	delete event;
}
ConsoleMethod(void, mouseClick) {
	CefMouseEvent* event = new CefMouseEvent();
	event->x = (int)round(atof(argv[1]) * renderHandler->getWidth());
	event->y = (int)round(atof(argv[2]) * renderHandler->getHeight());
	int clickType = atoi(argv[3]);
	browser->GetHost()->SendMouseClickEvent(*event, (cef_mouse_button_type_t)clickType, false, 1);
	browser->GetHost()->SendMouseClickEvent(*event, (cef_mouse_button_type_t)clickType, true, 1);

	delete event;
}

ConsoleMethod(void, mouseWheel) {
	CefMouseEvent* event = new CefMouseEvent();
	event->x = (int)round(atof(argv[1]) * renderHandler->getWidth());
	event->y = (int)round(atof(argv[2]) * renderHandler->getHeight());
	int deltaX = atoi(argv[3]);
	int deltaY = atoi(argv[4]);
	browser->GetHost()->SendMouseWheelEvent(*event, deltaX, deltaY);
	delete event;
}

ConsoleMethod(bool, bindToTexture) {
	TextureObject* tex = smTable[0];
	int cnt = 0;
	const char* search = argv[1];
	for (; tex; tex = tex->next) {
		if (tex == NULL) continue;
		cnt++;
		if (!tex->texFileName || !tex->texGLName) {
			BLBPrintf("An error has occurred at texture %u", cnt);
			continue;
		}
		if (strstr(tex->texFileName, search)) {
			*globalTextureID = tex->texGLName;
			BL_glBindTexture(GL_TEXTURE_2D, *globalTextureID);
			BL_glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, renderHandler->getWidth(), renderHandler->getHeight(), 0, GL_BGRA, GL_UNSIGNED_BYTE, texBuffer);
			browser->GetHost()->Invalidate(CefBrowserHost::PaintElementType::PET_VIEW);
			*isDirty = true;
			BLBPrintf("Found.");
			return true;
		}
	}
	return false;
}
ConsoleMethod(void, setTextureID) {
	*globalTextureID = atoi(argv[1]);
	BL_glBindTexture(GL_TEXTURE_2D, *globalTextureID);
	BL_glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, renderHandler->getWidth(), renderHandler->getHeight(), 0, GL_BGRA, GL_UNSIGNED_BYTE, texBuffer);
}
ConsoleMethod(void, setBrowserPage) {
	browser->GetMainFrame()->LoadURL(argv[1]);
}
ConsoleMethod(void, evalJS) {
	CefString s = browser->GetMainFrame()->GetURL();
	browser->GetMainFrame()->ExecuteJavaScript(argv[1], s, 0);
}

std::string ExePath() {
	char buffer[MAX_PATH] = { 0 };
	GetModuleFileNameA(NULL, buffer, MAX_PATH);
	std::string::size_type pos = std::string(buffer).find_last_of("\\/");
	return std::string(buffer).substr(0, pos);
}
DWORD WINAPI threadLoop(void* lpParam) {
	bool* doBreak = (bool*)lpParam;
	CefMainArgs args;
	CefSettings settings;
	settings.command_line_args_disabled = true;
	settings.no_sandbox = true;
	settings.windowless_rendering_enabled = true;
	settings.multi_threaded_message_loop = false;
	CefString(&settings.browser_subprocess_path).FromString(ExePath().append("\\CefClient.exe").c_str());
	// Whenever trying to use the above with just a simple "CefClient.exe" it instead tries to use Blockland.exe as the subprocess.
	settings.windowless_rendering_enabled = true;
	settings.command_line_args_disabled = true;
	settings.no_sandbox = true;
	CefString(&settings.user_agent).FromASCII("Mozilla/5.0 (Windows NT 6.2; WOW64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/65.0.3325.146 Safari/537.36 Blockland/r2033 (Torque Game Engine/1.3)");
	settings.log_severity = cef_log_severity_t::LOGSEVERITY_WARNING;
	if (!CefInitialize(args, settings, new BlockBrowser(), nullptr)) {
		BLBPrintf("Failed to initialize Blockland Browser.");
		return 0;
	}
	renderHandler = new BBR(2048, 2048);
	browser_client = new BrowserClient(renderHandler.get());

	CefWindowInfo window_info;
	window_info.windowless_rendering_enabled = true;
	window_info.SetAsWindowless(nullptr);
	window_info.bounds.x = 0;
	window_info.bounds.y = 0;
	window_info.bounds.width = 2048;
	window_info.bounds.height = 2048;
	window_info.shared_texture_enabled = false;
	CefBrowserSettings browser_settings;
	browser_settings.windowless_frame_rate = 120;
	browser_settings.javascript_access_clipboard = STATE_DISABLED;
	browser = CefBrowserHost::CreateBrowserSync(window_info, browser_client.get(), "about:blank", browser_settings, nullptr, nullptr);
	browser->GetHost()->WasResized();
	while (true) {
		if (*doBreak == false) {
			browser->GetHost()->CloseBrowser(true);
			CefShutdown();
			BLBPrintf("Blockland browser shut down.");
			return 0;
		}
		CefDoMessageLoopWork();
		Sleep(1);
	}
	return 0;
}

struct Point2I {
	int x, y;
};
BlFunctionDef(int, __stdcall, swapBuffers);
int __stdcall swapBuffersHook();
BlFunctionHookDef(swapBuffers);
int __stdcall swapBuffersHook() {
	if (*globalTextureID && *isDirty) {
		*isDirty = false;
		BL_glBindTexture(GL_TEXTURE_2D, *globalTextureID);
		BL_glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, renderHandler->getWidth(), renderHandler->getHeight(), GL_BGRA, GL_UNSIGNED_BYTE, texBuffer);
		BL_glTexParameteri(GL_TEXTURE_2D, GL_GENERATE_MIPMAP, GL_TRUE);
		BL_glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
		BL_glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
		BL_glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		BL_glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
	}
	int k;
	swapBuffersHookOff();
	k = swapBuffers();
	swapBuffersHookOn();
	return k;
}
ConsoleMethod(void, setDirty) {
	browser->GetHost()->Invalidate(CefBrowserHost::PaintElementType::PET_VIEW);
	*isDirty = true;
}
// Entirely a debug command, leaving for troubleshooting.
ConsoleMethod(void, randomizeBuffer) {
	for (int x = 0; x < renderHandler->getWidth(); x++) {
		for (int y = 0; y < renderHandler->getHeight(); y++) {
			for (int z = 0; z < 4; z++) {
				texBuffer[renderHandler->getWidth() * 4 * x + y * 4 + z] = rand();
			}
		}
	}
}
void ts_DumpTextures(ADDR obj, int argc, const char* argv[]) {
	BLBPrintf("Attempting to dump textures.");
	unsigned int cnt = 0;
	TextureObject* tex = smTable[0];
	for (; tex; tex = tex->next) {
		if (tex == NULL) continue;
		cnt++;
		if (!tex->texFileName || !tex->texGLName) {

			BLBPrintf("An error has occurred at texture %u", cnt);
			continue;
		}
		
		BLBPrintf("Texture [%u][%u]: %s", cnt, tex->texGLName, tex->texFileName);
	}
}

bool init() {
	*isDirty = false;
	BlInit;
	BLBPrintf("CEF attaching.");
	int n;
	BlScanHex(n, "C7 05 ? ? ? ? ? ? ? ? C7 05 ? ? ? ? ? ? ? ? E8 ? ? ? ? 8B 0D ? ? ? ? 8B F8");
	n += 2;
	BLBPrintf("Address of texturemanager is %08X", *(int*)n);
	smTable = (TextureObject**)*(int*)n;
	if (!tsf_InitInternal()) return false;
	tsf_AddConsoleFunc(NULL, NULL, "dumpTextures", ts_DumpTextures, "() - Dumps textures in the TextureManager.", 1, 1);
	RegisterMethod(resizeWindow, "(int width, int height) - Resizes window, not fully implemented.", 3, 3);
	RegisterMethod(mouseClick, "(float x, float y, int button) - Sends a click event at the given location.", 4, 4);
	RegisterMethod(keyboardEvent, "(char key, int modifiers) - Sends a keyboard event with modifiers.", 3, 3);
	RegisterMethod(mouseMove, "(float x, float y) - Send a mouse move event.", 3, 3);
	RegisterMethod(mouseWheel, "(float x, float y, int deltaX, int deltaY) - Sends a scroll event", 5, 5);
	RegisterMethod(bindToTexture, "(string search) - Attempts to bind the targeted texture.", 2, 2);
	RegisterMethod(setTextureID, "(int id) - Forces the texture id.", 2, 2);
	RegisterMethod(setDirty, "() - Sets the screen dirty.", 1, 1);
	RegisterMethod(randomizeBuffer, "() - Debugging purpose.", 1, 1);
	RegisterMethod(setBrowserPage, "(string url) - Loads the given url.", 2, 2);
	RegisterMethod(evalJS, "(string js) - Runs the given js on the frame.", 2, 2);
	tsf_AddVar("BLB::Build", new int(BLB_BuildNumber));
	tsf_Eval("function clientCmdCEF_goToURL(%a){setBrowserPage(%a);}"); // Support BLBrowser^2's URL call
	tsf_Eval("function clientCmdCEF_mouseMove(%a,%b){mouseMove(%a/2048,%b/2048);}"); // Convert mouseMove
	tsf_Eval("function clientCmdCEF_mouseClick(%a,%b,%c){mouseClick(%a/2048,%b/2048,%c);}"); // Convert mouseClick
	tsf_Eval("function clientCmdCEF_mouseWheel(%a,%b,%c,%d){mouseWheel(%a,%b,%c,%d);}"); // Support mouseWheel
	tsf_Eval("function BLB_OnUrlRequest(%url){if(!$BLB::DoNotSendUrls)commandToServer('BLB_UrlRequest', %url);}");
	tsf_Eval("function clientCmdBLB_MouseMove(%floatX, %floatY) {mouseMove(atof(%floatX), atof(%floatY));}");
	tsf_Eval("function clientCmdBLB_MouseClick(%floatX, %floatY, %button) {mouseClick(atof(%floatX), atof(%floatY), atoi(%button));}");
	tsf_Eval("function clientCmdBLB_SetBrowserPage(%string) {setBrowserPage(%string);}");
	tsf_Eval("function clientCmdBLB_bindToTexture(%string) {if(bindToTexture(%string)) {commandToServer('BLB_bindCallback', true);$BLB::TextureString = %string;} else commandToServer('BLB_bindCallback', false);}");
	tsf_Eval(
		"package BLB_ClientPackage {"
		"function connectToServer(%addr, %pass, %dir, %arr) {"
		"setBrowserPage(\"about:blank\");$BLB::TextureString = \"\";Parent::connectToServer(%addr, %pass, %dir, %arr);}"
		"function disconnectedCleanup(%rec) {"
		"setBrowserPage(\"about:blank\");$BLB::TextureString = \"\";Parent::disconnectedCleanup(%rec);}"
		"function optionsDlg::applyGraphics(%this) {"
		"Parent::applyGraphics(%this);if($BLB::TextureString !$= \"\")schedule(500, serverConnection, bindToTexture, $BLB::TextureString);}"
		"};"
	);
	*globalTextureID = 0;
	texBuffer = (char*)malloc(2048 * 2048 * 4);
	BLBPrintf("Attempting to initialize CEF.");
	initGL();
	BlockBrowserThread = CreateThread(NULL, 0, threadLoop, doBreakPtr, 0, NULL);
	BlScanFunctionHex(swapBuffers, "80 3D ? ? ? ? ? 74 06 FF 15 ? ? ? ? 80 3D ? ? ? ? ? 74 06 FF 15");
	swapBuffersHookOn();
	tsf_Eval("activatePackage(\"BLB_ClientPackage\");");
	return true;
}

bool deinit() {
	
	if (*doBreakPtr) {
		*doBreakPtr = false;
		WaitForSingleObject(BlockBrowserThread, INFINITE);
	}
	delete doBreakPtr;
	free(texBuffer);
	return true;
}
int __stdcall DllMain(HINSTANCE hInstance, unsigned long reason, void* reserved)
{
	switch (reason)
	{
	case DLL_PROCESS_ATTACH:
		return init();

	case DLL_PROCESS_DETACH:
		return deinit();

	default:
		return true;
	}
}

extern "C" void __declspec(dllexport) __cdecl placeholder(void) { }
