#include <include/base/cef_bind.h>
#include <include/cef_app.h>
#include <include/cef_parser.h>
#include <include/wrapper/cef_helpers.h>
#include <include/cef_base.h>

#include <include/cef_browser.h>
#include <include/cef_client.h>

#pragma comment(lib, "libcef.lib")
#pragma comment(lib, "libcef_dll_wrapper.lib")
class BlockBrowser : public CefApp {
public:
    BlockBrowser() {};
private:
    IMPLEMENT_REFCOUNTING(BlockBrowser);
};
int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PWSTR pCmdLine, int nCmdShow) {
    CefMainArgs main_args(hInstance);
    return CefExecuteProcess(main_args, new BlockBrowser(), nullptr);
}