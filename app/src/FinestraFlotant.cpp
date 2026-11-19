#include "FinestraFlotant.h"

#if JUCE_WINDOWS
 #ifndef WIN32_LEAN_AND_MEAN
  #define WIN32_LEAN_AND_MEAN
 #endif
 #ifndef NOMINMAX
  #define NOMINMAX
 #endif
 #include <windows.h>
#endif

void quedaSobreMibase (juce::Component& finestra)
{
    auto* principal = finestraMibase().getComponent();
    if (principal == nullptr || principal == &finestra) return;
   #if JUCE_WINDOWS
    auto* propietaria = static_cast<HWND> (principal->getWindowHandle());
    auto* aquesta = static_cast<HWND> (finestra.getWindowHandle());
    if (propietaria != nullptr && aquesta != nullptr)
        SetWindowLongPtrW (aquesta, GWLP_HWNDPARENT, reinterpret_cast<LONG_PTR> (propietaria));
   #else
    // A Linux (la sessió de proves) no cal: només es fa servir per compilar i provar.
    juce::ignoreUnused (finestra);
   #endif
}
