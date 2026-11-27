// 1.10: els fils de treball del mesclador (diversos nuclis) han de tenir la mateixa prioritat que l'àudio.
// A Windows: prioritat «temps crític» i el servei de Windows per a l'àudio (MMCSS, tasca «Pro Audio»), com fan els
// programes de música. Fitxer a part perquè <windows.h> no es barregi amb JUCE.
#include "mibase/MotorEstudi.h"

#if defined (_WIN32)
 #ifndef WIN32_LEAN_AND_MEAN
  #define WIN32_LEAN_AND_MEAN
 #endif
 #ifndef NOMINMAX
  #define NOMINMAX
 #endif
 #include <windows.h>

namespace
{
    void preparaFil()
    {
        SetThreadPriority (GetCurrentThread(), THREAD_PRIORITY_TIME_CRITICAL);
        using Fn = HANDLE (WINAPI*) (LPCWSTR, LPDWORD);
        if (HMODULE dll = LoadLibraryW (L"avrt.dll"))
            if (auto f = reinterpret_cast<Fn> (reinterpret_cast<void*> (GetProcAddress (dll, "AvSetMmThreadCharacteristicsW"))))
            {
                DWORD idx = 0;
                f (L"Pro Audio", &idx);
            }
    }
}

void instalaPrioritatFilsAudio() { mibase::Mesclador::preparaFilTreballador = &preparaFil; }
#else
void instalaPrioritatFilsAudio() {}
#endif
