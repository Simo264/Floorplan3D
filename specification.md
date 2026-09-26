L'obiettivo del progetto è creare un programma C++ che prenda come input un modello CAD 2D di una planimetria di una stanza o appartamento e creare una mesh 3D con i seguenti attributi: posizione, normale, tex coordinate. La mesh sarà esporta in formato GLTF. Il modello non deve contenere materiali.

Una volta esportato il file gltf, abbiamo uno script Python che genera la scena in Blender del modello. Qua vengono applicati i materiali, caricati gli asset delle porte e delle finestre, impostata camera, le luci, e altri parametri. Alla fine si procede con il rendering fotorealistico con Cycles.

# Assunzioni
La cosa più difficile di questo progetto è la mancanza di un unico standard tra i modelli CAD, il che rende molto difficile creare un algoritmo in grado di generalizzare tutti i modelli.

Ecco perché dobbiamo fare delle assunzioni: i muri devono già essere dei poligoni chiusi con un proprio spessore. Non posso avere una muro "rotto". Le porte e le finestre devono essere rappresentate come fori nelle pareti. I nomi dei layer e dei blocchi devono rispettare la stessa convenzione. Per i layer:
- A-WALL, A-DOOR, A-WINDOW

Per i blocchi:
- BLOCK-DOOR-[0-9], BLOCK-WINDOW-[0-9]

Inoltre verranno ignorati gli arredamenti. Sono presi in considerazione solo i muri, le porte e le finestre.

# Preprocessing
Prima ancora di eseguire il programma sarà necessario aprire il modello con un software come LibreCAD per renderlo consistente: correggere i nomi dei layer, dei blocchi, aggiungere segmenti per chiudere i poligoni dei muri, rimuovere del rumore dal modello.

# Parsing
Si raccolgono tutte le primitive (segmenti, polilinee, ecc.) riguardanti i muri, le porte e le finstre.

Le pareti sono molto spesso rappresentate come segmenti o come polilinee. 

Molto spesso le porte puntano allo stesso BLOCCO, che può essere lo stesso arco. Ma può accadere che una porta sia anche un insieme di segmenti o polilinee (nel caso delle porte di ingresso). 

Le finestre, come le porte, possono puntare allo stesso BLOCCO. Le finestre possono essere rappresentate in molti modi diversi, ma in genere sono rappresentate come segmenti.

# Vertex snapping
Questo viene fatto solo sui poligoni dei muri, e non invece sulle porte e le finestre che come vedremo dopo saranno trattate in modo diverso.

Viene utilizzata la struttura dati SpatialHash. I vertici vicini che sono inferiori a $\epsilon$ vengono compressi in un unico vertice. Questo passaggio è particolarmente utile sia per ridurre il numero di vertici da elaborare successivamente, sia per correggere eventuali problemi con vertici sovrapposti che potrebbero portare a problemi durante la creazione del grafo.

# Ricostruzione varchi
Qui bisogna chiudere i varchi per ricostruire poligoni semplici relativi a porte e finestre.

Per quanto riguarda le porte, conosciamo solo i segmenti che collegano i due bordi delle pareti. Per creare un poligono chiuso dobbiamo calcolare il secondo segmento parallelo.
Se una porta è rappresentata da una polilinea o da una serire di segmenti (come una porta di ingresso), prendiamo semplicemente uno dei due lati lunghi e calcoliamo il secondo segmento parallelo.

Le finestre, invece, sono più variabili e possono essere rappresentate in molti modi. Per questo motivo, l’approccio ideale si baserebbe sul clustering spaziale e sull’estrazione dei bounding box. Ogni cluster di punti rappresenterà una finestra; da ogni cluster calcolerò il riquadro di delimitazione ed estrarrò da esso un singolo segmento. Per fare ciò, considero semplicemente uno dei due lati lunghi del box, trovo i due vertici delle pareti e per derivare il secondo segmento seguo la procedura sopra descritta.

# PSLG + Half Edge 
Dopo che abbiamo costruito una rappresentazione completa della stanza/appartamento con poligoni dei muri, poligoni delle porte e poligoni delle finstre, andiamo a costruire un grafo planare PSLG.

Questo ci serve per estrarre e classificare ogni singola faccia. Devo sapere se una faccia rappresenta un muro, una porta o una finestra.
Perché in base a quello devo gestire l'estrusione in modo diverso: un muro viene estruso fino al soffitto, una porta viene estrusa solo nella parte superiore per creare il buco inferiore per inserirci l'asset della porta, una finestra viene estrusa sia dal basso che dall'alto in modo da creare un buco centrale.


# Creazione mesh
Per ottenere i vertici e gli indici necessari per costruire la mesh dobbiamo eseguire delle triangolazioni sulle facce. Utilizzo l'algoritmo di Delaunay vincolato.

Qua si procede con l'estrusione delle facce, calcolo delle normali e calcolo delle texture coordinate.

# Esportazione
Alla fine si esporta il modello in formato GLTF

# Rendering
Si eseguo lo script Python e si genera la scena Blender con la mesh, i materiali applicati, assets di porte/finestre, camera, luci e rendering con Cycles.

# Librarie e Software
- LibreCAD
- Blender
- Parsing: `libdxfrw`
- DBSCAN:  `SimpleDBSCAN`
- Half-Edge: `CGAL`
- CDT: `poly2tri`
