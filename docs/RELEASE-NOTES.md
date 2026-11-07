## PAUL3D 0.3.0

PAUL3D é uma versão modificada do [PS5MG](https://github.com/nikay99/PS5MG) de nikay99 (GPL-3.0), voltada para impressão 3D.

### Novo
- **Comandos de impressão no visualizador.** Abaixo dos botões STL/OBJ, escreva por exemplo `altura 12 cm, base redonda 3 mm`,
  `largura 80 mm e base quadrada 2 mm margem 5`, `escala 150%` ou `sem base`. A página mostra as medidas finais e a base no 3D antes de baixar.
- **Foto de trás (experimental).** Uma segunda foto, das costas do objeto, entra junto na geração (TRELLIS.2). Modo "Alternar"
  (mesmo tempo) ou "Média" (cerca de 2× mais lento) em More options. Na API: campos `back` e `multiview`.

### Corrigido
- O STL e o OBJ saíam de cabeça para baixo. Agora a peça fica em pé na mesa.

### Ao atualizar do PS5MG
- A pasta de dados agora é `/data/PAUL3D`. Renomeie `/data/PS5MG` para `/data/PAUL3D` pelo FTP antes de abrir: os modelos continuam valendo.
- O ID do app continua `PPSA99577`: o PAUL3D substitui o PS5MG no console.

### Antes de começar: os modelos
O PAUL3D não traz modelos e não baixa nada. Um comando no PC baixa dos autores e copia para o console: veja o README
(Windows: tools\setup-weights.bat; macOS e Linux: ./tools/setup-weights.sh com o endereço do console). Cerca de 6,5 GB.

### Não testado nesta versão
- A foto de trás ainda não foi testada no console: compare com e sem ela, mesma seed.

## PAUL3D 0.2.4

### New
- **Look at a mesh on the TV.** When the title is idle, press Cross on the controller: the newest mesh appears, textured, and the left
  stick turns it. L2/R2 zoom, L1/R1 pick another mesh, Circle goes back. The screen shows the button hints.
- No job starts while a mesh is on the TV. It closes itself after two minutes without input.

### Before you start: the model weights
PAUL3D has no model weights of its own and never downloads any. One command on your PC gets them from their authors and copies them to
the console: see the README (Windows: double-click tools\setup-weights.bat; macOS and Linux: ./tools/setup-weights.sh with your console's address).
About 6.5 GB. TRELLIS.2 and Pixal3D are MIT; DINOv3 has Meta's own licence.

### Unchanged
- The title has no network client and downloads nothing.
- The title restarts itself after every job.
- Pixal3D with texture at 1024 can stop without an error message.
- Not tested for this release: how the view feels with a long list of meshes, and meshes from before 0.2.3 over 64 MB (they are refused).
