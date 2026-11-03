# Mibase

DAW (estudi de gravació) personal per a **Windows**, fet a mida per a un sol músic i pensat per crear.
Neix de les idees provades a **Riffs**, una app d'Android per gravar idees musicals i muntar-les en un estudi multipista, ara amb el que dona un ordinador:

- **ASIO** i poc retard (també WASAPI).
- **Teclats i controladors MIDI**.
- **Targetes d'àudio** externes.
- **Plugins VST3** (instruments i efectes).

Motor propi escrit en C++; [JUCE](https://juce.com) es fa servir només per als controladors d'àudio i MIDI, els plugins i les finestres.

> Estat: **0.3 — estudi (edició)**. Es poden crear projectes, fer pistes, importar àudio i editar els clips. Encara no sona ni grava (arriba a la 0.4).

## Com es fa servir

### Pantalla d'inici
Mibase s'obre a la **pantalla d'inici**, en tres columnes:

- **Esquerra, les carpetes.** A dalt, els llocs (Escriptori, Documents, Música, Baixades); al mig, l'arbre de carpetes, amb el nombre de projectes que té cada una; a baix, les unitats. Clica una carpeta per triar-la: és on es crearan els projectes nous (es veu a baix de tot). La fletxa obre o tanca una carpeta. «Carpeta nova aquí» en crea una dins la triada. Les carpetes que ja són projectes no surten a l'arbre.
- **Centre, els projectes recents**, el més recent primer. Un clic el tria; doble clic (o Retorn) l'obre. «Busca un projecte» filtra pel nom i «Només d'aquesta carpeta» mostra només els de la carpeta triada. «Obre un altre projecte…» obre qualsevol fitxer `.mibase`.
- **Dreta, el projecte triat**: «Obre a l'estudi», quantes pistes i preses té, el tempo i l'**evolució del projecte** (les últimes línies del seu diari). Si el projecte ja no es troba (s'ha mogut o el disc no hi és), ho diu i el pots treure de la llista.
- **Amplades**: arrossega la ratlla entre dues columnes per fer-les més amples o més estretes (doble clic: mida inicial). Es recorda per a la propera vegada.

### Projecte nou
«Projecte nou» demana el **nom** (avisa si Windows no l'accepta o si ja hi és), el **tempo**, el **compàs** i amb què comences (de moment, projecte buit; les plantilles arribaran més endavant). A la dreta es veu què es crearà:

```
<carpeta triada>\
   <Nom>\
      <Nom>.mibase     el projecte (text; es desa sol)
      Evolució.txt     diari del projecte, el més recent a dalt
      Àudios\          les gravacions: «Guitarra - presa 01.wav», «Veu - presa 03.wav»…
      Còpies\          còpies automàtiques del fitxer del projecte (les 10 últimes; els àudios no es copien)
```

Els àudios conserven sempre el seu nom, encara que després canviïs el nom de la pista.

### Estudi
L'estudi té quatre seccions d'esquerra a dreta: **Pistes** (noms), **Volum** (un fader per pista), la **línia de temps** (regla i clips) i el **panell de la pista triada**. Arrossega la ratlla entre dues seccions per fer-les més amples o més estretes (doble clic: mida inicial); es recorda.

- **Barra de dalt**: capçal a l'inici, reproduir i gravar (encara desactivats), la posició (compàs.temps; el compàs 0 és on comença la cançó), el metrònom (groc = encès), el tempo i el compàs (clica per canviar el tempo), la graella **Compàs / Temps / Lliure**, Desfés i Refés, i **Importa àudio…**. Al mig, el text d'informació mostra el valor del que arrossegues.
- **Pista nova**: «+ Pista nova» a sota de l'última pista (Guitarra, Veu o Àudio). També pots **arrossegar fitxers d'àudio** des de l'Explorador (WAV, AIFF, FLAC, MP3, OGG): van a la pista on els deixes (o a pistes noves) i es copien a `Àudios` amb el nom «Pista - presa NN.wav» (els que no són WAV es converteixen a WAV de 24 bits).
- **Capçalera de pista**: M (silenci), S (solo) i el botó de gravar (només una pista alhora). Doble clic al nom: canviar-lo (els àudios conserven el seu nom). Arrossega la capçalera amunt o avall per ordenar les pistes. Botó dret: canviar el nom, el tipus (color) o esborrar la pista.
- **Clips** (com a Riffs): clic = triar-lo; Ctrl+clic = afegir-ne o treure'n; Maj+clic = tots els d'entremig. Arrossegar: el primer moviment decideix; de costat el mou en el temps (s'enganxa a la graella per la bandera, o per l'inici si no en té); amunt o avall el passa a una altra pista sense moure'l en el temps. Amb un sol clip triat: **cantonades de baix** = retallar (fins i tot més enllà del so: és silenci), **cantonades de dalt** = fade in i fade out, **punt del mig** = volum del clip (0 a −24 dB). El fitxer no es toca mai.
- **Bandera del primer temps**: botó dret sobre el clip → «Posa la bandera del primer temps aquí»; es pot arrossegar pel triangle. En mode **Lliure**, si mous un clip amb la bandera visible, la bandera es queda quieta i l'àudio llisca per sota.
- **Regla**: clic = capçal; arrossegar = marcar el **bucle** (compassos sencers, en groc); doble clic dins el bucle = encendre'l o apagar-lo; botó dret = opcions.
- **Ratolí i teclat**: roda = amunt i avall; Maj+roda = de costat; Ctrl+roda = zoom del temps; Alt+roda = alçada de les pistes. Supr = treure clips; Ctrl+D = duplicar; Ctrl+Z = desfer; Ctrl+Y = refer; Esc = no triar res; Inici = capçal a l'inici.
- Tot es desa sol. El primer cop que es desa en cada sessió, i després com a molt cada 10 minuts, es guarda una còpia del fitxer del projecte a `Còpies`.

### Barra de dalt (a totes les pantalles)
**Inici**, **Estudi · nom del projecte** (quan n'hi ha un d'obert) i **Diagnòstic**. Al mig, l'estat de la targeta d'àudio (controlador, targeta, freqüència, buffer i, si l'has mesurat, el retard).

### Diagnòstic

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
3. **Abans de descomprimir-lo**: botó dret sobre el .zip → **Propietats** → marca **Desbloqueja** → **D'acord**. Així Windows no avisa cada cop que l'obres (el programa no està signat).
4. Descomprimeix-lo on vulguis i obre `Mibase.exe`. No cal instal·lar res.
5. Per a ASIO, cal el controlador de la targeta d'àudio (Scarlett: «Focusrite Control»; Alesis: «Steinberg built-in ASIO Driver», ASIO4ALL o FlexASIO).

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
| `motor/` | El motor (ara: clic, so de prova, vúmetres, mesura de latència, projectes al disc, edició de l'estudi). Sense interfície: es prova sol. |
| `app/` | El programa de Windows (finestres, àudio, MIDI). La icona és a `app/recursos/`. |
| `proves/` | Proves automàtiques del motor. |
| `extern/asiosdk/` | Kit d'ASIO oficial de Steinberg (2.3.4), només les parts necessàries. |
| `extern/nlohmann/` | Biblioteca JSON (per llegir i desar els projectes). |

## Historial de versions
*(el més recent a dalt)*

- **0.3.0** — Estudi: pistes (Guitarra, Veu, Àudio) amb M/S/gravar, faders, ordenar i canviar el nom; importar àudio (botó o arrossegant fitxers) a `Àudios`; clips amb moure (en el temps o de pista), retallar, fades, volum, bandera del primer temps (fixa en mode Lliure), duplicar i treure; graella Compàs/Temps/Lliure; regla amb bucle; zoom i alçada amb la roda; Desfés/Refés; tot es desa sol. Seccions d'amplada ajustable. Tema més clar i accent taronja. Icona pròpia. Arreglat el separador de la pantalla d'inici que no redibuixava bé.
- **0.2.0** — Pantalla d'inici: navegador de carpetes, projectes recents (cercador i filtre per carpeta), detalls del projecte amb la seva evolució, columnes d'amplada ajustable. Projecte nou (nom, tempo, compàs) que crea la carpeta, el fitxer `.mibase`, `Evolució.txt`, `Àudios` i `Còpies`. Barra de dalt amb Inici / Estudi / Diagnòstic i l'estat de la targeta. Estudi provisional. Diagnòstic: l'entrada WASAPI només surt amb WASAPI, els talls desconeguts es diuen amb paraules i els canals actius pel nom.
- **0.1.0** — Pantalla de diagnòstic: triar controlador (ASIO/WASAPI), targeta, freqüència i buffer; clic de prova amb tempo; vúmetres de les entrades; teclats MIDI amb llista de missatges i so de prova; mesura de la latència real d'anada i tornada; estat (càrrega i talls); historial; «Copia el diagnòstic». Compilació automàtica per a Windows amb GitHub Actions.
- **0.0** — Repositori creat.

## Llicències
- Mibase: GPLv3 (fitxer `LICENSE`), perquè fa servir el kit d'ASIO amb llicència GPLv3.
- JUCE 8 — AGPLv3 / llicència JUCE.
- ASIO SDK de Steinberg — GPLv3 (`extern/asiosdk/LICENSE.txt`). ASIO és una marca de Steinberg Media Technologies GmbH.
- nlohmann/json 3.11.3 — MIT.
- VST 3 SDK de Steinberg (inclòs a JUCE) — MIT. VST és una marca de Steinberg Media Technologies GmbH.
