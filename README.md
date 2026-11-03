# Mibase

DAW (estudi de gravació) personal per a **Windows**, fet a mida per a un sol músic.
Neix de les idees provades a **Riffs**, una app d'Android per gravar idees musicals i muntar-les en un estudi multipista, ara amb el que dona un ordinador:

- **ASIO** i poc retard (també WASAPI).
- **Teclats i controladors MIDI**.
- **Targetes d'àudio** externes.
- **Plugins VST3** (instruments i efectes).

Motor propi escrit en C++; [JUCE](https://juce.com) es fa servir només per als controladors d'àudio i MIDI, els plugins i les finestres.

> Estat: **en preparació**. Encara no hi ha cap versió per provar.

## Com es fa servir
*(s'anirà omplint amb cada versió)*

## Com s'instal·la
1. A la pestanya **Actions** del repositori, obre l'última compilació correcta (marca verda).
2. A baix, a **Artifacts**, baixa el .zip de Mibase.
3. Descomprimeix-lo on vulguis i obre `Mibase.exe`. No cal instal·lar res.
4. Per a ASIO, cal el controlador de la targeta d'àudio (o un de genèric com ASIO4ALL o FlexASIO).

## Com es compila
Es compila sol amb **GitHub Actions** a cada pujada (workflow `.github/workflows/windows.yml`).

Per compilar-lo a mà a Windows (Visual Studio 2022 i CMake):

```
cmake -B build -G "Visual Studio 17 2022"
cmake --build build --config Release
```

## Estructura
| Carpeta | Què hi ha |
|---|---|
| `motor/` | El motor (projecte, temps, clips, mescla, efectes, sintes). Sense interfície: es prova sol. |
| `app/` | El programa de Windows (finestres, àudio, MIDI, VST3). |
| `proves/` | Proves automàtiques del motor. |

## Historial de versions
*(el més recent a dalt)*

- **0.0** — Repositori creat.

## Llicències de tercers
- JUCE 8 — llicència JUCE (modalitat gratuïta).
- VST 3 SDK de Steinberg — MIT. VST és una marca de Steinberg Media Technologies GmbH.
- ASIO SDK de Steinberg — GPLv3. ASIO és una marca de Steinberg Media Technologies GmbH.
