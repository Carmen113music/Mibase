# Mibase

DAW (estudi de gravació) personal per a **Windows**, fet a mida per a un sol músic i pensat per crear.
Neix de les idees provades a **Riffs**, una app d'Android per gravar idees musicals i muntar-les en un estudi multipista, ara amb el que dona un ordinador:

- **ASIO** i poc retard (també WASAPI).
- **Teclats i controladors MIDI**.
- **Targetes d'àudio** externes.
- **Plugins VST3** (instruments i efectes).

Motor propi escrit en C++; [JUCE](https://juce.com) es fa servir només per als controladors d'àudio i MIDI, els plugins i les finestres.

> Estat: **0.1 — pantalla de diagnòstic**. Serveix per comprovar la targeta, el teclat MIDI i el retard real abans de començar a gravar.

## Com es fa servir (0.1)
Mibase s'obre a la **pantalla de diagnòstic**:

1. **Targeta d'àudio**: tria el *Controlador* (ASIO és el més ràpid; amb la Scarlett, «Focusrite USB ASIO»), la *Targeta*, la *Freqüència* i el *Buffer*. Com més petit el buffer, menys retard (però més risc de talls). «Panell del controlador» obre el de la targeta (només ASIO). Els ajustos es desen sols.
2. **Proves de so**: «Clic de prova» fa sonar un metrònom (el primer temps més agut); el tempo es canvia amb la barra. «So del teclat» fa sonar un so senzill quan toques el teclat MIDI.
3. **Entrades**: un vúmetre per cada entrada activa. Si es posa vermell, l'entrada satura: abaixa el guany a la targeta.
4. **Teclat MIDI**: s'activen sols tots els teclats connectats. A la llista surt tot el que envien: notes, pads, potenciòmetres (controls) i inflexió.
5. **Latència real**: connecta un cable de la sortida 1 a l'entrada 1 de la targeta (o posa el micròfon davant d'un altaveu, amb el volum baix) i prem «Mesura la latència». Sona un soroll curt 5 vegades i et diu el retard real d'anada i tornada, comparat amb el que diu el controlador. Es desa per a cada configuració (targeta, freqüència i buffer): servirà per alinear les gravacions.
6. **Estat**: la configuració actual, la càrrega i els talls d'àudio.
7. **Copia el diagnòstic**: copia tota la informació per enganxar-la al xat si alguna cosa no va.

## Com s'instal·la
1. A la pestanya **Actions** del repositori, obre l'última compilació correcta (marca verda).
2. A baix, a **Artifacts**, baixa `Mibase-windows`.
3. Descomprimeix-lo on vulguis i obre `Mibase.exe`. No cal instal·lar res.
4. Per a ASIO, cal el controlador de la targeta d'àudio (Scarlett: «Focusrite Control»; Alesis: «Steinberg built-in ASIO Driver», ASIO4ALL o FlexASIO).

## Com es compila
Es compila sol amb **GitHub Actions** a cada pujada (workflow `.github/workflows/windows.yml`), i hi passa les proves del motor.

Per compilar-lo a mà a Windows (Visual Studio 2022 i CMake):

```
cmake -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release
ctest --test-dir build -C Release
```

Només el motor i les proves (qualsevol sistema, sense JUCE): `cmake -B build -DMIBASE_APP=OFF`.

## Estructura
| Carpeta | Què hi ha |
|---|---|
| `motor/` | El motor (ara: clic, so de prova, vúmetres, mesura de latència). Sense interfície: es prova sol. |
| `app/` | El programa de Windows (finestres, àudio, MIDI). |
| `proves/` | Proves automàtiques del motor. |
| `extern/asiosdk/` | Kit d'ASIO oficial de Steinberg (2.3.4), només les parts necessàries. |

## Historial de versions
*(el més recent a dalt)*

- **0.1.0** — Pantalla de diagnòstic: triar controlador (ASIO/WASAPI), targeta, freqüència i buffer; clic de prova amb tempo; vúmetres de les entrades; teclats MIDI amb llista de missatges i so de prova; mesura de la latència real d'anada i tornada; estat (càrrega i talls); historial; «Copia el diagnòstic». Compilació automàtica per a Windows amb GitHub Actions.
- **0.0** — Repositori creat.

## Llicències
- Mibase: GPLv3 (fitxer `LICENSE`), perquè fa servir el kit d'ASIO amb llicència GPLv3.
- JUCE 8 — AGPLv3 / llicència JUCE.
- ASIO SDK de Steinberg — GPLv3 (`extern/asiosdk/LICENSE.txt`). ASIO és una marca de Steinberg Media Technologies GmbH.
- VST 3 SDK de Steinberg (inclòs a JUCE) — MIT. VST és una marca de Steinberg Media Technologies GmbH.
