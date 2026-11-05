# Mibase

DAW (estudi de gravació) personal per a **Windows**, fet a mida per a un sol músic i pensat per crear.
Neix de les idees provades a **Riffs**, una app d'Android per gravar idees musicals i muntar-les en un estudi multipista, ara amb el que dona un ordinador:

- **ASIO** i poc retard (també WASAPI).
- **Teclats i controladors MIDI**.
- **Targetes d'àudio** externes.
- **Plugins VST3** (instruments i efectes).

Motor propi escrit en C++; [JUCE](https://juce.com) es fa servir només per als controladors d'àudio i MIDI, els plugins i les finestres.

> Estat: **0.12 — Millores d'edició, Chord Pad i notes fantasma**. Abans, **0.11 — Carpetes de pistes** i barra de dalt nova. Abans, **0.10 — Chord Pad com un piano**: 12 pads com les tecles d'una octava, el teclat MIDI toca acords, inversions a l'editor d'acord, notes fantasma triades arrossegant pistes. Abans, **0.9 — Chord Pad** i **0.8 — Harmonia**: tonalitat (detectada o triada), 13 escales amb mode visual, assistit o bloquejat, acords de la tonalitat, constructor d'acords i progressions al panell de l'esquerra; diversos editors MIDI alhora. Abans, **0.7 — MIDI i Piano Roll**: pistes d'instrument amb el sinte intern, Piano Roll complet (dibuixar, editar, quantitzar, velocitats, notes fantasma), gravació des del teclat MIDI. Els plugins VST3 arriben a la versió següent.

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
- **Fitxer**: Preferències (targeta, MIDI i latència) · Desa (Ctrl+S) · Nova versió a partir d'aquí (com una branca: una còpia del projecte a la mateixa carpeta, que comparteix els àudios) · Projecte nou · Obre un projecte · Tanca el projecte · Importa àudio (Ctrl+I) · Exporta el projecte (una carpeta amb el projecte i **només** els àudios que fa servir, per obrir-lo en un altre ordinador) · Exporta la mescla (un WAV estèreo de 24 bits amb el que sona: el bucle, si és encès; si no, del primer clip al darrer; sense la claqueta) · Surt.
- D'esquerra a dreta: **Mibase** i la versió, **Inici**, el **nom del projecte** obert (torna a l'estudi) i el menú **Fitxer**. Les **Preferències** (targeta d'àudio, MIDI i latència: l'antic Diagnòstic) són dins el menú Fitxer.
- **Transport**, fix a la dreta quan hi ha un projecte obert (no a la pantalla d'inici): al principi, enrere un compàs, endavant un compàs, atura, reprodueix (verd), grava (vermell) i **bucle** (groc; també la tecla **L**: l'encén o l'apaga; si no n'hi ha cap, en marca un de 4 compassos al capçal, que pots moure i allargar a la regla). Al costat, la **pantalla** amb el compàs i el temps.
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
- **Pista nova**: «+ Pista nova» → Àudio mono o estèreo, o **Instrument MIDI** (bateria, acords i bus d'efectes vindran després). Cada pista nova agafa un color diferent; amb el botó dret sobre el nom es canvia el color, mono/estèreo, l'**entrada per gravar** (E1, E2… o E1-2 en estèreo; surt al costat de M S gravar), el nom o s'esborra. També pots **arrossegar fitxers d'àudio** des de l'Explorador.
- **Ona**: es dibuixa plena, com una ona analògica. Amb molt de zoom es veuen les mostres i la línia del zero.
- **Clips un damunt de l'altre**: sona només el de dalt (el darrer que hi has posat, per exemple una presa nova). Si el silencies amb la X, torna a sonar el de sota.
- **Ratolí**: roda = amunt i avall; Maj+roda = de costat (mai abans del començament); **Ctrl+roda = zoom del temps**, a partir del punt on tens el ratolí, que queda centrat a la pantalla; Alt+roda = alçada de les pistes (mai tan petites que no es vegin M, S i gravar).
- **Capçal**: es mou clicant (o arrossegant) la **regla**; clicar les pistes ja no el mou.
- **Teclat**: Espai = reprodueix / atura; R = grava; **Z** = tot el projecte a la pantalla; **Maj+Z** = el que hi ha triat; **M** = silencia el triat (o la pista); **Q / W / E** = graella Compàs / Temps / Lliure; Ctrl+C / Ctrl+X / Ctrl+V; Insert = enganxar inserint o trepitjant; Supr = treure clips; Ctrl+D = duplicar; Ctrl+Z / Ctrl+Y = desfer / refer; Ctrl+S = desar; Ctrl+I = importar; 1–6 = eines; Esc = res triat i fletxa; Inici = al principi.
- **Faders i panorama**: arrossega amunt i avall; amb **Maj**, deu vegades més fi (per posar, per exemple, -0,2 dB). Doble clic: 0 dB (o el panorama al mig). Els vúmetres tenen la mateixa escala que el fader; si la pista té el botó de gravar encès, també mostren el que entra per la targeta.
- **Taula de mescles**: una tira per pista amb els llocs per als **inserts** (efectes) i els **enviaments** a busos (arribaran més endavant), el **panorama**, M, S, gravar, el fader amb vúmetre i l'entrada; i, fixa a la dreta, la **Sortida** (volum general i vúmetre).
- Tot es desa sol (i una còpia del projecte a `Còpies`, com a molt cada 10 minuts).

### Carpetes de pistes
- El botó de la **carpeta** (a dalt de la columna dels noms de pista) mostra o amaga el **panell de carpetes**, a l'esquerra dels noms. Sense carpetes, hi surten totes les pistes seguides.
- **Carpeta nova** (botó de dalt del panell), o tria unes quantes pistes amb **Ctrl** (o **Maj**, per triar-les seguides) clicant-ne el nom i prem **Ctrl+G**: demana el nom i les posa juntes dins la carpeta.
- **Arrossega el nom d'una pista** del panell a una carpeta per posar-la-hi, o fora de les carpetes per treure-la'n. Les pistes d'una carpeta queden sempre seguides.
- **Clic al nom de la carpeta**: en tria totes les pistes (amb Ctrl o Maj, també les d'altres carpetes). Amb diverses pistes triades, **M** les silencia totes.
- La **fletxa** plega o desplega la carpeta: plegada, a la taula de treball totes les seves pistes es veuen com **una sola fila** (amb els clips de cada pista en franges fines, i M i S per a totes); doble clic a la fila la desplega. L'**ull** l'amaga de la taula de treball (continua sonant). Doble clic al nom (o botó dret): canviar-li el nom o esborrar-la (les pistes es queden).

### Pistes d'instrument (MIDI)
- **Pista nova → Instrument MIDI** fa una pista que sona amb el **sinte intern** de Mibase. Amb el botó dret al nom → *Instrument* tries el so: Piano, Piano elèctric, Orgue, Pad, Baix, Pinçat, Solista o Cordes (els plugins VST3 arribaran a la versió següent, al mateix lloc).
- **Clips MIDI**: amb el **llapis** (2) a una pista d'instrument fas un clip buit (arrossegant, o d'un compàs) i s'obre al Piano Roll. Els clips MIDI es mouen, es retallen, es tallen amb les tisores, s'ajunten amb la cola, es copien i s'enganxen com els d'àudio (no passen a pistes d'àudio, ni al revés). Dins el clip es veuen les notes en petit. **Doble clic** (o el menú del botó dret) l'obre al Piano Roll.
- **Teclat MIDI**: quan la pista triada és d'instrument, el teclat (l'AKAI) la fa sonar. Si la pista triada és d'àudio, sona el so de prova del Diagnòstic, com abans. Si tens més d'un teclat, a la secció **Pista** del panell de l'esquerra pots dir quin teclat toca cada pista: aquella pista la toca sempre aquell teclat (encara que no estigui triada), i els altres teclats no la toquen.
- **Gravar MIDI**: encén el botó vermell d'una pista d'instrument i prem Grava (R). Sona un compàs de compte i es grava el que toques al teclat, amb la velocitat, el pedal (sustain), la roda de modulació i el pitch bend. El que toques durant el compte queda amagat a l'esquerra del clip (no s'esborra).
- **Canviar el tempo**: els clips MIDI segueixen els compassos (les notes es guarden en temps musical); els d'àudio no es mouen.
- La **dinàmica** d'un clip MIDI és la velocitat de les notes: els clips MIDI no tenen fades ni volum de clip (el fader de la pista sí).

### Piano Roll
S'obre a **baix de l'estudi** (doble clic a un clip MIDI, o la tecla **P** per mostrar-lo o amagar-lo). La ratlla de sobre el fa més alt o més baix. El botó de la finestra el **desenganxa** en una finestra a part (per portar-lo a la segona pantalla i maximitzar-lo); el mateix botó, o tancar la finestra, el torna a baix. Es recorda com el vas deixar.

- **Diversos editors**: el **cadenat** bloqueja un editor amb el seu clip. Si després obres un altre clip MIDI (doble clic, o fent-ne un amb el llapis), s'obre en un **editor nou a sota** (fins a 3). Tots tenen la mateixa alçada i **el mateix zoom i desplaçament en el temps**: els compassos queden alineats d'un a l'altre. L'editor **actiu** (el de la franja taronja a l'esquerra) és el que fa servir el panell d'harmonia. La fletxa avall tanca aquell editor. Un editor només canvia de clip quan fas **doble clic** a un altre clip MIDI (triar-lo amb un clic no el canvia).
- Amb un editor obert, la **columna esquerra** mostra el **panell d'harmonia** en lloc del fader de la pista (i es fa més ampla; la seva amplada es recorda a part).

- **Teclat** a l'esquerra amb el nom de les notes (do central = **C4**). Clica-hi per escoltar-les (més a la dreta de la tecla, més fort); mentre sona la cançó, s'il·luminen les que sonen.
- **Graella** sincronitzada amb el tempo i el compàs del projecte: compassos, temps i les divisions de la graella. La part de fora del clip es veu més fosca (el que hi ha fora no sona, però no s'esborra).
- **Eines** (tecles 1–3): **Fletxa**: clica per triar (Ctrl: afegeix o treu, Maj: afegeix), arrossega per moure (en el temps i en to), arrossega la **vora dreta** per allargar o escurçar (o l'esquerra, per l'inici), arrossega en un lloc buit per fer un **requadre**; **doble clic** fa una nota (o l'esborra). **Llapis**: clica per fer una nota; arrossega cap a la dreta per fer-la més llarga. **Goma**: esborra les notes per on passes. Botó dret en una nota: menú (copiar, duplicar, quantitzar, velocitat, octaves); en un lloc buit, torna a la fletxa.
- **Graella**: Lliure, 1/1, 1/2, 1/4, 1/8, 1/16, 1/32 i tresets (1/4 T, 1/8 T, 1/16 T). **Alt** mentre arrossegues: sense graella. **Nota**: la durada de les notes noves (igual que la graella o una de fixa).
- **Quantitza** (o **Q**): les notes triades (o totes, si no n'hi ha cap) a la graella de la cançó. **Maj+Q**: també el final. Al menú del botó dret, també «a mitges» (50 %).
- **Vel**: la velocitat de les notes noves (arrossega amunt i avall; doble clic: 100).
- **Notes fantasma** (botó blau): per defecte no se'n veu cap. **Clica el botó** (el cursor es torna un fantasma) i **clica un clip de la taula de treball**: MIDI (notes discontínues) o àudio (l'ona, tènue). Tornar a clicar el mateix clip el treu. També pots **arrossegar el nom d'una pista MIDI** (a la columna dels noms de l'estudi) fins al botó i en veuràs les notes en línia discontínua, com a referència (si hi arrossegues la mateixa pista, els seus altres clips). Pots afegir-ne diverses. **Clic** al botó: un menú per triar-les o treure-les. Cada editor té les seves.
- **Pestanyes + i -** a l'esquerra del teclat (o Re Pàg / Av Pàg): la vista una octava amunt o avall.
- **Notes / Chord Pad** (interruptor de la barra): en **Chord Pad**, el carril de velocitats deixa lloc a **12 pads d'acords posats com les tecles d'una octava de piano**: 7 a baix (les blanques, C D E F G A B) i 5 a dalt (les negres, C# D# F# G# A#); cada pad porta a la cantonada la nota que el toca. **Amb el Chord Pad obert, el teclat MIDI (i el teclat del Piano Roll) toquen acords**: cada tecla toca el pad de la seva nota, en qualsevol octava (C3, C4 o C5 toquen el pad C). Si graves, es graven les notes de l'acord. **Clic** a un pad: sona l'acord. **Arrossega'l** al Piano Roll i deixa'l anar on vulguis: s'hi posa l'acord, amb la durada triada (1 compàs, 1/2, 1/4). **Octava**, **Tancat / Obert** i **Moviment mínim** (cada acord es col·loca a prop de l'anterior) com al constructor. Un punt taronja vol dir que l'acord té notes de fora de l'escala.
  - **Selector de presets** (botó de l'esquerra): *Acords de la tonalitat*: a les blanques, els set graus (C = I, D = ii, E = iii…); a les negres, acords de color (en major: V7, ii7, iv, bVI i bVII; en menor: V7, im7, IV, V i bII). O una de les **50 seqüències** de cançons (Pop, rock i indie / Funk): a les blanques, els acords de la cançó en ordre i sense repetir (si en sobra lloc, els de la seva tonalitat); a les negres, acords de color.
  - **Edita** (llapis), o el botó dret sobre un pad: obre l'**editor d'acord** com el de Cubase: *Fonamental*, *Tipus* (maj, min, dim, sus4, sus2, aug), *Tensions* (7, j7, b9, 9, #9, 11, b5/#11, #5/b13, 6/13) i *Nota greu*; A dalt: *Escolta* el fa sonar, *Buida* deixa el pad buit, **− / +** canvien la **inversió** (+: la nota de baix puja una octava; −: la de dalt baixa; fins a 3) i **Aplica** (o Retorn) tanca l'editor. Els canvis es desen al pad a l'instant. Al pad, «inv +1» vol dir que està invertit.
  - Els pads es desen amb la pista (cada pista té els seus). Es poden desfer.
- **Velocitats** (carril de baix): una barra per nota. Arrossega-la amunt o avall; si la nota és triada amb d'altres, es mouen totes juntes. En un lloc buit, arrossega per «pintar» velocitats; amb **Maj**, fa una línia recta (crescendo o diminuendo). Si hi ha notes triades, només els afecta a elles. La vora de dalt del carril el fa més alt.
- **Notes trepitjades** (dues del mateix to una damunt l'altra): ratllades i amb la vora vermella.
- **Teclat de l'ordinador**: Supr = esborra; Ctrl+A = totes; Ctrl+C / Ctrl+X / Ctrl+V (enganxa al capçal); Ctrl+D = duplica just després; Amunt / Avall = un semitò (amb Maj, una octava); Esquerra / Dreta = mou un pas de graella (amb Maj, més llarga o més curta); Q / Maj+Q = quantitza; Z = el clip a la pantalla; Ctrl+Z / Ctrl+Y = desfer / refer; Esc = res triat. Espai i R fan com a l'estudi (també amb el Piano Roll en una finestra a part).
- **Ratolí**: roda = amunt i avall; Maj+roda = de costat; Ctrl+roda = zoom del temps (a partir del ratolí); Alt+roda = alçada de les notes. Clicar la regla mou el capçal.
- Tot el que fa el Piano Roll són notes MIDI normals: es desen al projecte, es poden desfer i sonen igual amb el sinte intern que (més endavant) amb els VST3.

### Panell de l'esquerra (amb un editor obert)
Cada secció es **plega i es desplega clicant-ne el títol** (es recorda).
- **Pista** (la de l'editor actiu): l'**instrument** (ara el sinte intern; els VST3 arribaran aquí), el **so** (Piano, Orgue…) i el **teclat MIDI** que la toca: *Qualsevol* (el que toca la pista triada, com sempre) o un teclat concret. Si aquell teclat no hi és connectat, es diu «(desconnectat)» i es guarda igualment.
- **Tonalitat**: en **Automàtica**, Mibase la dedueix de totes les notes MIDI de la cançó (major o menor) i diu com n'està de segur; si encara no hi ha prou notes, ho diu. Clicant una nota (C, C#…) o l'escala la tries a mà (deixa de ser automàtica; el botó la torna a detectar). Canviar la tonalitat **no toca cap nota**. A sota, les notes de l'escala (la fonamental en taronja).
- **Escales**: Major, Menor natural, Menor harmònica, Menor melòdica, Dòrica, Frígia, Lídia, Mixolídia, Eòlica, Lòcria, Pentatònica major, Pentatònica menor i Blues.
- **Escala al Piano Roll**: **Visual** (les files de fora de l'escala es veuen més fosques, la fonamental amb un toc taronja i una marca a les tecles), **Assistit** (una nota nova o moguda fora de l'escala va a la més propera de l'escala; les fletxes amunt i avall pugen o baixen un grau de l'escala) o **Bloquejat** (no deixa posar notes de fora de l'escala ni moure-les-hi).
- **Ressalta l'acord que sona**: pinta de verd les notes de l'acord que sona al capçal (de totes les pistes MIDI) i en diu el nom a la regla del Piano Roll i al panell (Am, C/E, G7…).
- **Acords de la tonalitat**: els set (tríades o setenes). **Arrossega'n un** fins al Piano Roll i deixa'l anar al temps que vulguis. O bé clic: l'escoltes i el pots posar al Piano Roll amb un altre clic (es veu en verd sota el ratolí; amunt o avall canvia d'octava). **Maj+clic** en posa més; Esc o el botó dret ho deixa. **Durada de cada acord**: 1 compàs, 1/2, 1/4 o la nota del Piano Roll. **Moviment mínim**: cada acord es col·loca a prop de l'anterior.
- **Constructor d'acords**: fonamental, tipus (major, menor, sus2, sus4, disminuït, augmentat, maj7, m7, 7, m7b5, 9, maj9, m9, 11, 13, add9, 6, m6), inversió, voicing tancat o obert, octava, doblar una octava i baix. *Escolta*, i *Posa'l*: **arrossega'l** fins al Piano Roll i deixa'l al temps que vulguis (o clica'l i després clica al Piano Roll). Sobre les **notes triades** al Piano Roll: *Fes-ne acords d'aquest tipus*, *Fes-ne acords de la tonalitat* (cada nota es converteix en l'acord que li toca), *Inv. avall / Inv. amunt* (inversions dels acords triats) i *Enllaça* (els acords triats es reordenen perquè es moguin el mínim).
- **Progressió**: Pop, Balada, Anys 50, Rock, Indie, Folk, Jazz, Èpica, Andalusa, Trista i Blues de 12, amb els acords de la tonalitat (si la cançó és menor, les progressions «majors» fan servir la relativa: el Pop a La menor és C G Am F). 2, 4, 8 o 16 compassos. *Escolta* la fa sonar al tempo; *Insereix* la posa al clip de l'editor actiu a partir del compàs del capçal (o del començament del clip), un acord per compàs.
- Tot el que es posa són **notes MIDI normals**: es poden editar, desfer i sonen amb qualsevol instrument.

### Preferències (abans, Diagnòstic)

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
| `motor/` | El motor (clic, vúmetres, mesura de latència, projectes al disc, edició, MIDI i sinte intern, i el motor de l'estudi: mescla, instruments, gravació i WAV). Sense interfície: es prova sol. |
| `app/` | El programa de Windows (finestres, àudio, MIDI). La icona és a `app/recursos/`. |
| `proves/` | Proves automàtiques del motor. |
| `extern/asiosdk/` | Kit d'ASIO oficial de Steinberg (2.3.4), només les parts necessàries. |
| `extern/nlohmann/` | Biblioteca JSON (per llegir i desar els projectes). |

## Historial de versions
*(el més recent a dalt)*

- **0.12.0** — Fletxa: un clip que no estava triat només es tria (arrossegant, requadre: així es pot triar un tros a Temps o Lliure); la mà per moure surt només sobre un clip ja triat. **Z / Maj+Z** fan el zoom de la finestra on és el ratolí (Piano Roll o taula de treball; al Piano Roll, Maj+Z = les notes triades). **Preferències** en una finestra a part amb botó de tancar. Piano Roll sense el botó «Nota» (les notes noves fan el pas de la graella). Panell d'harmonia sense el constructor d'acords ni «Acord al capçal»; el ressaltat de l'acord que sona sempre encès; botó **Aplica al Chord Pad** a la progressió. Presets de cançons al Chord Pad **per ordre** a partir de la C (C, C#, D…), sense repetir i la resta buits. Carpeta **Tonalitat** al selector de presets (nota i escala). **Notes fantasma**: clica el botó i després el clip de la taula de treball (MIDI, o àudio: se'n veu l'ona); un altre cop el mateix clip, el treu.
- **0.11.0** — **Carpetes de pistes**: panell de carpetes (botó de la carpeta), carpeta nova o Ctrl+G amb diverses pistes triades, arrossegar pistes a les carpetes, clic al nom per triar-ne totes les pistes, plegar (una sola fila a la taula de treball, amb M i S per a totes) i amagar. Pistes triades amb Ctrl o Maj; M les silencia totes. **Barra de dalt** nova: Mibase i versió, Inici, nom del projecte i Fitxer; fora la pastilla de l'estat de la targeta; el Diagnòstic passa a ser **Fitxer → Preferències**. Botó de **bucle** al transport (i tecla L). Les carpetes es desen al projecte.
- **0.10.0** — Millores del Piano Roll i del Chord Pad. **Notes fantasma**: per defecte cap; arrossega el nom d'una pista MIDI al botó per veure-les (clic al botó: triar-les o treure-les). El Piano Roll **només canvia de clip amb doble clic** (abans canviava només de triar un altre clip). **Chord Pad** amb 12 pads posats com les tecles d'un piano (7 blanques a baix, 5 negres a dalt) i, amb el Chord Pad obert, **el teclat MIDI i el del Piano Roll toquen l'acord del pad de cada nota** (també quan graves). **Editor d'acord**: fora la casella d'escriure; botons de dalt més petits, **− / +** per a la inversió i **Aplica** per tancar-lo. Els pads desats amb la 0.9 (16) es passen sols a la disposició nova.
- **0.9.0** — Chord Pad (fase 3 del MIDI). Interruptor Notes / Chord Pad al Piano Roll: 16 pads d'acords que sonen amb un clic i s'arrosseguen al Piano Roll (també a la finestra separada); editor d'acord com el de Cubase (fonamental, tipus, tensions, nota greu o escrit a mà); 50 presets de seqüències de cançons (pop, rock, indie i funk) i els acords de la tonalitat. Panell de l'esquerra amb seccions plegables i una secció nova **Pista** (instrument, so i quin teclat MIDI la toca, si en tens més d'un). Els acords de la tonalitat i el *Posa'l* del constructor ara s'arrosseguen al temps que vulguis. La barra del Piano Roll s'estreny quan cal perquè el cadenat i el desenganxar no quedin mai tapats.
- **0.8.0** — Harmonia (fase 2 del MIDI). Amb un editor obert, la columna esquerra mostra el panell d'harmonia en lloc del fader: tonalitat detectada de les notes (o triada a mà), 13 escales, mode Visual / Assistit / Bloquejat al Piano Roll, acord que sona al capçal (nom i notes en verd), acords de la tonalitat, constructor d'acords (18 tipus, inversions, voicings, octaves), convertir notes en acords, inversions i enllaç amb el mínim moviment, progressions de 2 a 16 compassos (escoltar i inserir). Diversos editors MIDI alhora (fins a 3), amb cadenat per bloquejar-los i el mateix zoom i temps. Pestanyes d'octava + i - a l'esquerra del teclat. El sinte intern té més marge i un limitador suau (els acords grans no saturen).
- **0.7.0** — MIDI i Piano Roll (fase 1). Pistes d'instrument MIDI amb el sinte intern (8 sons). Clips MIDI no destructius (notes en temps musical, 960 per negra) que es mouen, retallen, tallen, ajunten, copien i enganxen com els d'àudio. Piano Roll a baix de l'estudi o en una finestra a part: teclat amb noms de notes, graella sincronitzada, fletxa / llapis / goma, moure, allargar per l'inici o pel final, requadre, copiar, enganxar, duplicar, esborrar, transposar, quantitzar (graella, tresets, a mitges, inici i final), zoom horitzontal i vertical, octaves, notes fantasma, notes trepitjades marcades, carril de velocitats (una barra per nota, en grup, a mà alçada o en línia). El teclat MIDI fa sonar la pista d'instrument triada i grava notes, pedal, modulació i pitch bend. Canviar el tempo manté els clips MIDI als seus compassos. Exportar la mescla inclou el MIDI. Les notes mai queden penjades (aturar, bucle, saltar, esborrar la pista o canviar d'instrument mentre sona). Punts d'endoll preparats per als VST3 i per a les fases següents (escales, generadors, controladors).
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
