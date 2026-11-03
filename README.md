# Mibase

DAW (estudi de gravació) personal per a **Windows**, fet a mida per a un sol músic i pensat per crear.
Neix de les idees provades a **Riffs**, una app d'Android per gravar idees musicals i muntar-les en un estudi multipista, ara amb el que dona un ordinador:

- **ASIO** i poc retard (també WASAPI).
- **Teclats i controladors MIDI**.
- **Targetes d'àudio** externes.
- **Plugins VST3** (instruments i efectes).

Motor propi escrit en C++; [JUCE](https://juce.com) es fa servir només per als controladors d'àudio i MIDI, els plugins i les finestres.

> Estat: **0.6 — editar com a casa**: sona i grava, i ara amb requadre de selecció, copiar i enganxar (trepitjant o inserint), colors i dreceres de teclat.

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

### Barra de dalt
- **Fitxer**: Desa (Ctrl+S) · Nova versió a partir d'aquí (com una branca: una còpia del projecte a la mateixa carpeta, que comparteix els àudios) · Projecte nou · Obre un projecte · Tanca el projecte · Importa àudio (Ctrl+I) · Exporta el projecte (una carpeta amb el projecte i **només** els àudios que fa servir, per obrir-lo en un altre ordinador) · Exporta la mescla (un WAV estèreo de 24 bits amb el que sona: el bucle, si és encès; si no, del primer clip al darrer; sense la claqueta) · Surt.
- **Inici · Estudi · Diagnòstic**, i l'estat de la targeta d'àudio.
- **Transport**, fix a la dreta quan hi ha un projecte obert (no a la pantalla d'inici): al principi, enrere un compàs, endavant un compàs, atura, reprodueix (verd) i grava (vermell). Al costat, la **pantalla** amb el compàs i el temps.
  - **Reprodueix** (o Espai) des del capçal. Mentre sona, clicar la regla fa saltar la música; si el bucle és encès, el repeteix.
  - **Atura** (o Espai) torna on havies començat, per tornar-ho a escoltar o gravar. Si ja està aturat, va al principi.
  - **Grava** (o R) a la pista que té el **botó vermell** encès: sona **un compàs de compte** amb la claqueta (encara que estigui apagada) i comença a gravar al capçal. La presa nova («Pista - presa NN.wav») queda amb la bandera al primer temps, i el que has tocat durant el compte hi és (pots allargar el clip cap a l'esquerra). Es corregeix sol el **retard de la targeta**: el que has mesurat al Diagnòstic o, si no l'has mesurat, el que diu el controlador.

### Estudi
D'esquerra a dreta: la **columna esquerra** (pestanyes *Taula de treball* / *Taula de mescles* i, a sota, la pista triada: panorama, fader amb vúmetre, M, S i gravar), **Pistes** (amb un vúmetre petit a cada una), la **línia de temps** i el **panell** del clip triat. Arrossega la ratlla entre dues seccions per fer-les més amples o més estretes (doble clic: mida inicial); es recorda.

- **Barra d'eines**: la **caixa d'eines** (tecles 1 a 6; botó dret en un lloc buit = fletxa):
  1. **Fletxa**: tria i mou (també clips de pistes diferents: Ctrl+clic n'afegeix o en treu). **Clicar en un lloc buit** treu la selecció; **arrossegar en un lloc buit** fa un **requadre**. Amb un sol clip triat, les cantonades de baix retallen, les de dalt fan els fades i el punt del mig canvia el volum del clip.
  2. **Llapis**: clica i arrossega per fer una finestra nova; si no arrossegues, la fa d'un compàs.
  3. **Goma**: clica (o passa per sobre) per esborrar clips.
  4. **Tisores**: talla el clip on cliques (segons la graella).
  5. **Cola**: clica un clip i arrossega fins al del costat: en fa una sola finestra. Si són trossos seguits del mateix àudio, s'ajunten sense tocar res; si són preses diferents o hi ha un forat, es crea un àudio nou amb tot (els originals no es toquen).
  6. **X**: silencia o torna a fer sonar un clip.
- **Requadre** (botó al costat de Compàs / Temps / Lliure): apagat, tria **els clips sencers** que toca; encès, tria **només el tros que queda dins** (enganxat a la graella), encara que els clips en sobresurtin. Supr buida aquest tros.
- **Copiar i enganxar**: Ctrl+C copia (els clips sencers o només el tros), Ctrl+X retalla, **Ctrl+V enganxa al capçal**, a les mateixes pistes, amb el començament del que has copiat al capçal. El botó del costat (o la tecla **Insert**) tria com s'enganxa: apagat **trepitja** el que hi ha; encès **insereix**: tot el que hi ha després del capçal, a totes les pistes, s'aparta.
- **Color**: el quadret de color de la barra pinta els clips triats o, si no n'hi ha, la pista. Colors preparats o la roda (to i saturació) amb la barra de lluminositat.
- **Clips trepitjats**: on un clip queda damunt d'un altre es veu ratllat (i sona el de dalt).
- **Claqueta**: la tecla l'encén i l'apaga; la fletxeta obre el volum (0 a 150 %), el so (Digital, Fusta, Analògic, Campana) i «Escolta-la» per sentir-la al tempo del projecte. Al costat, el tempo i el compàs (clica per canviar el tempo), la graella **Compàs / Temps / Lliure**, Desfés i Refés. A la dreta, el text d'informació.
- **Pista nova**: «+ Pista nova» → Àudio mono o estèreo (instrument, bateria, acords i bus d'efectes vindran després). Cada pista nova agafa un color diferent; amb el botó dret sobre el nom es canvia el color, mono/estèreo, l'**entrada per gravar** (E1, E2… o E1-2 en estèreo; surt al costat de M S gravar), el nom o s'esborra. També pots **arrossegar fitxers d'àudio** des de l'Explorador.
- **Ona**: es dibuixa plena, com una ona analògica. Amb molt de zoom es veuen les mostres i la línia del zero.
- **Clips un damunt de l'altre**: sona només el de dalt (el darrer que hi has posat, per exemple una presa nova). Si el silencies amb la X, torna a sonar el de sota.
- **Ratolí**: roda = amunt i avall; Maj+roda = de costat (mai abans del començament); **Ctrl+roda = zoom del temps**, a partir del punt on tens el ratolí, que queda centrat a la pantalla; Alt+roda = alçada de les pistes (mai tan petites que no es vegin M, S i gravar).
- **Capçal**: es mou clicant (o arrossegant) la **regla**; clicar les pistes ja no el mou.
- **Teclat**: Espai = reprodueix / atura; R = grava; **Z** = tot el projecte a la pantalla; **Maj+Z** = el que hi ha triat; **M** = silencia el triat (o la pista); **Q / W / E** = graella Compàs / Temps / Lliure; Ctrl+C / Ctrl+X / Ctrl+V; Insert = enganxar inserint o trepitjant; Supr = treure clips; Ctrl+D = duplicar; Ctrl+Z / Ctrl+Y = desfer / refer; Ctrl+S = desar; Ctrl+I = importar; 1–6 = eines; Esc = res triat i fletxa; Inici = al principi.
- **Faders i panorama**: arrossega amunt i avall; amb **Maj**, deu vegades més fi (per posar, per exemple, -0,2 dB). Doble clic: 0 dB (o el panorama al mig). Els vúmetres tenen la mateixa escala que el fader; si la pista té el botó de gravar encès, també mostren el que entra per la targeta.
- **Taula de mescles**: una tira per pista amb els llocs per als **inserts** (efectes) i els **enviaments** a busos (arribaran més endavant), el **panorama**, M, S, gravar, el fader amb vúmetre i l'entrada; i, fixa a la dreta, la **Sortida** (volum general i vúmetre).
- Tot es desa sol (i una còpia del projecte a `Còpies`, com a molt cada 10 minuts).

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
| `motor/` | El motor (clic, vúmetres, mesura de latència, projectes al disc, edició, i el motor de l'estudi: mescla, gravació i WAV). Sense interfície: es prova sol. |
| `app/` | El programa de Windows (finestres, àudio, MIDI). La icona és a `app/recursos/`. |
| `proves/` | Proves automàtiques del motor. |
| `extern/asiosdk/` | Kit d'ASIO oficial de Steinberg (2.3.4), només les parts necessàries. |
| `extern/nlohmann/` | Biblioteca JSON (per llegir i desar els projectes). |

## Historial de versions
*(el més recent a dalt)*

- **0.6.0** — Requadre de selecció (clips sencers o només el tros de dins) i selecció de diverses pistes. Copiar, retallar i enganxar al capçal, trepitjant o inserint (Insert). Clicar en un lloc buit treu la selecció; el capçal només es mou a la regla. Botó dret en un lloc buit = fletxa. Color de clips i pistes (preparats i roda). Clips trepitjats ratllats. Tecles Z, Maj+Z, M, Q, W, E.
- **0.5.0** — Sona i grava. Reproducció de totes les pistes (volum, panorama, silenci, solo, fades, volum de clip, bucle) amb la claqueta enganxada als temps; el clip de dalt tapa els de sota. Gravació a la pista amb el botó vermell, amb un compàs de compte, l'entrada triada per pista i el retard de la targeta corregit; el fitxer s'escriu al disc des d'un fil a part (el so no s'atura mai a esperar el disc). Vúmetres a les pistes, a la columna esquerra i a la taula de mescles. Panorama. Sortida amb volum general. Exportar la mescla (WAV). Faders sense imant al 0 dB i amb Maj per afinar; capçal de fader ample amb ratlletes. Taula de mescles amb llocs per a inserts i enviaments. Sense la columna de volums (ja hi ha el fader a l'esquerra). El transport només surt dins d'un projecte. No es pot desplaçar abans del començament.
- **0.4.0** — Menú Fitxer (desar, nova versió, nou, obrir, tancar, importar, exportar el projecte, sortir). Transport fix a la dreta amb pantalla de posició. Caixa d'eines: fletxa, llapis, goma, tisores, cola i X. Claqueta amb volum i quatre sons (es pot escoltar). Columna esquerra amb taula de treball / de mescles i el fader de la pista triada; taula de mescles. Ona analògica amb detall fins a les mostres i la línia del zero; zoom centrat. Pistes d'àudio mono o estèreo amb colors. Aspecte d'aparell antic (tecles, pantalla, faders). Alçada mínima de les pistes.
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
