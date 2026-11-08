## PAUL3D 0.7.0

### O PAUL3D agora é só "peça por texto"
- Saiu tudo do 3D por foto: o motor TRELLIS.2, a foto de trás, o visualizador de malhas na TV e os modelos 3D (6 GB que você
  pode apagar de `/data/PAUL3D/models/q4` pelo FTP). O app ficou bem menor.
- A página que abre em `http://<console>:7870` é a geração de peças por texto, com o painel Modelos embaixo.
- Saiu a opção de usar outro servidor de IA (PS5LM ou PC): a IA é sempre a do console.

### Desempenho
- **O modelo é carregado quando o app abre e fica na memória.** Só o primeiro pedido depois de abrir espera a carga
  (Qwen3.8 27B: uns 2 minutos); os seguintes começam na hora. Antes, o app descarregava o modelo depois de 5 minutos parado.
- O app lembra o último modelo usado e carrega esse ao abrir.
- **Menos memória por pedido:** contexto de 4096 tokens e lote de 512 (antes 8192 e 2048). Com o 27B ocupando 9,8 GB, isso
  deixa folga na GPU; numa versão anterior o app reiniciou sozinho por falta de memória.

### Atenção
- Ainda não foi testado no console: me diga como o 27B se comporta em vários pedidos seguidos.

## PAUL3D 0.6.1

### Novo
- **Escolha do modelo de texto.** Na "Peça por texto", em Servidor da IA, o campo "Modelo do console" lista todos os `.gguf` instalados
  (em `/data/PAUL3D/models/text` e no PS5LM). Trocar o modelo faz o console carregar o outro (o app reinicia; o 1º pedido demora).
- No painel Modelos, a IA de texto mostra qual modelo está em uso, também quando é um que você copiou por FTP.

### Corrigido
- A "Peça por texto" desistia depois de 60 s sem resposta, e um modelo grande (Qwen3.8 27B) leva uns 2 minutos para carregar.
  Agora ela espera até 5 minutos.

### Atenção
- O Qwen3.8 27B (9,8 GB) ocupa quase toda a memória da GPU (sobram 0,1 GiB). Num teste o app reiniciou sozinho no 4º pedido seguido;
  se acontecer, use o Qwen3.5 9B.

## PAUL3D 0.6.0

### Novo
- **Modelos, baixados pelo próprio console.** Na página inicial, a seção Modelos mostra o que está instalado e baixa o que falta,
  direto do Hugging Face: marque "Li a licença", clique em Baixar e acompanhe a barra. Se a conexão cair ou você cancelar,
  "Continuar" segue de onde parou. Cada arquivo é conferido (sha256) antes de ser usado.
- **A IA de texto dentro do PAUL3D.** A "Peça por texto" usa o próprio console: baixe a "IA de texto (Qwen3.5 9B)" em Modelos e
  pronto, sem PS5LM e sem PC. O modelo fica carregado enquanto você faz pedidos; o app reinicia sozinho quando você gera uma malha
  ou depois de 5 minutos parado.
- Outros programas podem usar a mesma IA pela API da OpenAI (`/v1/chat/completions`).

### Rede
- O PAUL3D agora acessa a internet, mas só quando você pede um download em Modelos, só no huggingface.co e só os arquivos fixos
  do catálogo.

### Teste pedido
1. Em Modelos, baixe a "IA de texto (Qwen3.5 9B)" (5,7 GB) e anote a velocidade.
2. Em "Peça por texto", gere o chaveiro de exemplo só com o PAUL3D aberto.
3. Depois gere uma malha 3D, para conferir que o 3D continua igual.

## PAUL3D 0.5.0

Só uma troca por dentro, preparando o PAUL3D para ter texto e imagem no mesmo app: o motor 3D agora usa a mesma versão da
biblioteca ggml que o gerador de imagens (0.27). **Nada muda na tela.** A malha deve sair igual à da 0.4.0.

### Teste pedido
Com a mesma foto, seed 42, Balanced e textura desligada, gere uma malha na 0.4.0 e outra na 0.5.0, e compare:
`python3 tools/compare-glb.py v040.glb v050.glb` (deve dizer PASS) e os tempos de `tools/mg.sh log`.
Se algo piorar, volte para a 0.4.0 (o zip continua na página de versões).

## PAUL3D 0.4.0

### Novo
- **Peça por texto** (`http://<console>:7870/cad`, ou o arquivo `ps5/cad.html` aberto no PC): descreva uma peça funcional com
  medidas ("caixa 60 × 40 × 25 mm com tampa, parede 2 mm") e uma IA escreve o código OpenSCAD; o navegador gera o STL e mostra a
  peça. Erros do OpenSCAD voltam para a IA corrigir (até 3 vezes); depois é só escrever o que mudar. O código fica editável.
- A IA é qualquer servidor com a API da OpenAI: o PS5LM no console (`http://<console>:8081/v1`) ou o llama.cpp no PC.
  Sem servidor, a página mostra como instalar, com os links do modelo (Qwen3.5 9B / Qwen2.5-Coder 7B).
- Botão "Peça por texto" no topo da página e o logo do PAUL3D no lugar do antigo.

### Observação
- O PS5LM e o PAUL3D não rodam juntos no console: com o PS5LM aberto, use `ps5/cad.html` pelo PC.
- O navegador precisa de internet uma vez para o three.js, o OpenSCAD (11 MB) e uma fonte.

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
