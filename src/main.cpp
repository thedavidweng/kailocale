#include "kailocale/app_shell.hpp"
#include "platform/switch2_platform.hpp"

int main() {
    kailocale::Switch2PlatformServices platform;
    kailocale::Switch2LanguageService language_service;
    kailocale::AppShell shell(platform, language_service);
    const kailocale::ShellOutcome outcome = shell.run();
    return outcome.succeeded() ? 0 : 1;
}
