# Hoover UFERSA

Rover (carrinho) controlado pelo celular via Wi-Fi, desenvolvido na UFERSA.
O ESP-01S cria a própria rede Wi-Fi e mostra uma página de controle. Não precisa de roteador, internet nem aplicativo.

```
Celular ──Wi-Fi──> ESP-01S ──4 sinais PWM──> Ponte H MX1508 ──> 2 motores N20
                     ▲                            ▲
                  3,3V (AMS1117)               9V (bateria)
```

## Material

| Item | Observação |
|---|---|
| ESP-01S | Tem que ser o **S**: ele já tem os resistores de pull-up em EN e RST |
| Ponte H MX1508 (módulo mini) | Aguenta até ~10V na alimentação dos motores |
| Módulo regulador AMS1117-3.3 | Linear: esquenta, porque transforma em calor a diferença entre 9V e 3,3V |
| 2 motores N20 de **6V** | O firmware limita o PWM para não passar de ~6V médios |
| Bateria de 9V + chave liga/desliga | A chave fica no fio **negativo** |
| Placa de face simples + barras de pinos fêmea | Todos os módulos são encaixados, nada é soldado direto na placa |

## Ligações (placa)

| ESP-01S | Ponte H MX1508 | Motor |
|---|---|---|
| TX (GPIO1) | IN1 | MotorA |
| RX (GPIO3) | IN2 | MotorA |
| GPIO0 | IN3 | MotorB |
| GPIO2 | IN4 | MotorB |
| VCC | ← saída 3,3V do regulador | |
| GND | ← GND geral (via **jumper** na placa) | |

Alimentação: bateria (+) → VIN do regulador e VCC da MX1508. Bateria (−) → chave P4 → GND de todos os módulos.

No código, **GPIO0/GPIO2 são o motor esquerdo** (MotorB na placa) e **TX/RX são o motor direito** (MotorA).

### Cuidados na montagem

- **O jumper de GND do ESP é obrigatório.** Na placa, o GND do ESP não tem trilha. Sem o fio, o ESP fica sem terra e não liga.
- **Orientação do regulador:** o VIN vai no pad quadrado, a saída de 3,3V no meio e o GND na ponta. Se ficar invertido, os 9V entram onde não devem.
- **Orientação do ESP-01S:** o TX vai no pad quadrado, e o VCC do ESP no pad ligado à saída do regulador.
- **Antes de colocar a bateria:** com o multímetro em continuidade, confira o jumper de GND e se não há curto entre VCC e GND.
- **Recomendado:** um capacitor eletrolítico de 220–470 µF entre 3,3V e GND, perto do ESP. Ele evita que o ESP reinicie quando os motores arrancam.

## Gravando o firmware

O código atual é [`CodigoHooverESPNOW/CodigoROVER.cpp`](CodigoHooverESPNOW/CodigoROVER.cpp).

1. Instale a [Arduino IDE](https://www.arduino.cc/en/software).
2. Em **Arquivo → Preferências → URLs adicionais**, adicione:
   `https://arduino.esp8266.com/stable/package_esp8266com_index.json`
3. Em **Ferramentas → Placa → Gerenciador de placas**, instale **esp8266** (ESP8266 Community).
4. Crie uma pasta `CodigoROVER` com o arquivo `CodigoROVER.ino` e cole nele o conteúdo do `CodigoROVER.cpp`. A Arduino IDE só abre arquivos `.ino`.
5. Escolha a placa **Generic ESP8266 Module**, com Flash Size **1MB (FS:64KB OTA:~470KB)** (o padrão).
6. **Tire o ESP-01S da placa do rover** e encaixe num gravador USB para ESP-01. Os pinos TX/RX vão para os motores, então não dá para gravar com ele no rover.
7. Coloque o gravador em modo de gravação: GPIO0 no GND ao ligar. Muitos gravadores têm uma chave **PROG/UART** para isso.
8. Clique em **Carregar**. Depois volte a chave para UART (ou desconecte o GPIO0), tire o ESP do gravador e encaixe de volta no rover.

**Uso de memória** (compilado com o pacote esp8266 3.1.2, sem nenhum aviso do compilador):

| Memória | Usado | Total | |
|---|---|---|---|
| Programa (flash) | 331 KB | 958 KB | 35% |
| RAM (variáveis globais) | 28,7 KB | 80 KB | 35% |
| IRAM (código rápido do núcleo/Wi-Fi) | 60,7 KB | 65,5 KB | 92%: é o normal do pacote ESP8266 com Wi-Fi, não vem do código do rover |

A página de controle (~24 KB) fica na flash (`PROGMEM`), não na RAM.

## Como usar

1. Ligue a chave do rover.
2. No celular, conecte ao Wi-Fi **`ROVER - UFERSA`** (senha `12345678`).
3. Abra o navegador em **http://192.168.4.1**.
4. Deixe o celular na horizontal e toque no ícone de tela cheia, no canto superior direito. No Android, isso esconde a barra do navegador.
   - **No iPhone**, o Safari não tem esse botão de tela cheia. Use **Compartilhar → Adicionar à Tela de Início**: o ícone "Hoover" que aparece abre o controle sem as barras do Safari.
   - Se o Android avisar que a rede **não tem internet**, escolha **manter conectado**. Senão ele troca para outra rede e o controle para de responder.

**Tipo de controle:** escolha **Botões** ou **Joystick** no topo da tela. O celular lembra a escolha.

- **Modo Botões:**
  - **◀ ▶ (polegar esquerdo):** direção. Sozinhos, giram o rover no próprio eixo. Junto com ▲ ou ▼, fazem curva.
  - **▲ ▼ (polegar direito):** frente e ré. O botão de ré fica laranja quando apertado.
- **Modo Joystick:** um único controle, com o polegar esquerdo. Quanto mais longe do centro, mais rápido.
  - Para cima/baixo: frente e ré.
  - Para os lados: gira no lugar.
  - Nas diagonais: curvas, mais fechadas quanto mais de lado.
  - O centro tem uma folga, e os eixos "puxam" o dedo, para ficar fácil andar reto. A bola fica laranja na ré.

**No resto da tela:**

- **Velocidade (Lenta / Média / Máx.):** limita a potência a 40%, 70% ou 100% do máximo. Vale nos dois modos.
- **Painel central:** mostra o que o rover está fazendo e a potência que cada motor recebe. Azul é frente, laranja é ré. Ajuda a encontrar motor ligado ao contrário.
- **Indicador de alcance (barras no canto superior direito):** mostra a qualidade da conexão com o rover. Detalhes abaixo.
- **No computador:** dá para dirigir com as setas ou com W A S D, nos dois modos.

### Indicador de alcance

O navegador não consegue medir a distância até o rover em metros: o ESP8266 não informa a força do sinal, e o navegador não deixa ler a força do Wi-Fi. Por isso o indicador mostra a **qualidade da conexão**. Perto do limite do alcance, o Wi-Fi começa a repetir pacotes, as respostas demoram e alguns comandos se perdem.

| Barras | Texto | Significado |
|---|---|---|
| 4 verdes | Sinal ótimo | Respostas rápidas, nenhuma perda |
| 3 verdes | Sinal bom | Alguma demora ou uma perda isolada |
| 2 laranjas | Sinal fraco | Mais de 10% de perdas ou respostas acima de 200 ms |
| 1 vermelha | No limite | Mais de 25% de perdas ou respostas acima de 400 ms. **O celular vibra** e a tela pede para se aproximar |
| nenhuma | Sem sinal | O rover parou de responder |

- **Como é calculado:** o indicador usa as respostas dos últimos 3 segundos, então se recupera logo quando você volta para perto.
- **Por que as barras só caem com o rover andando:** com o rover parado, o celular economiza bateria "cochilando" o Wi-Fi, e as respostas demoram mesmo de perto. Por isso o tempo de resposta só conta enquanto você dirige.
- **Para conhecer o alcance do seu rover:** afaste-se dirigindo devagar até aparecer "No limite". Esse é o raio seguro naquele lugar. Paredes, pessoas e o rover andar perto do chão diminuem o alcance.

### Segurança

- **O rover para sozinho** se ficar mais de 400 ms sem receber comando: sinal perdido, celular travado ou bateria do celular acabando.
- **A página solta todos os botões** se você trocar de app, bloquear a tela ou sair do navegador.

## Ajustes no código

| Constante | Para que serve |
|---|---|
| `ssid` / `password` | Nome e senha da rede do rover. **Troque a senha** se mais de um rover for usado no mesmo lugar |
| `TENSAO_BATERIA` | Tensão da bateria. Se trocar por 2 células 18650 em série, use `7.4` e o limite de PWM se ajusta sozinho |
| `TENSAO_MOTOR` | Tensão nominal dos motores (`6.0` para N20 de 6V) |
| `FATOR_GIRO` | Força do giro no próprio eixo (`0.8` = 80% da velocidade) |
| `400` no `loop()` | Tempo (ms) sem comando até o rover parar |

**Diagnóstico:** abra **http://192.168.4.1/status** para ver a memória livre do ESP, a fragmentação, o tamanho do programa e quantos celulares estão conectados. Para conferir se há vazamento de memória, dirija alguns minutos e recarregue: a memória livre deve ficar estável.

**Protocolo:** a página envia `GET /joy?x=<-1..1>&y=<-1..1>&v=<0.2..1>`.
- `x` é a direção e `y` é frente/ré. Nos botões valem -1, 0 ou 1; no joystick, qualquer valor intermediário.
- `v` é o fator de velocidade.
- A página reenvia o comando a cada 100 ms enquanto o rover anda.

O firmware mistura `x` e `y` numa fórmula contínua. Para os botões, ela dá exatamente o mesmo resultado da versão anterior. No joystick, ela passa suavemente de "girar no lugar" para "curva".

## Problemas comuns

| Sintoma | Causa provável |
|---|---|
| O ESP reinicia (a rede some e volta) quando o rover arranca | A bateria de 9V não aguenta o pico de corrente. Coloque o capacitor de 220–470 µF no 3,3V ou troque a bateria por 2× 18650 |
| O ESP não liga ou a rede nunca aparece | Falta o jumper de GND, o ESP ou o regulador está encaixado ao contrário, ou o GPIO0/GPIO2 está sendo puxado para baixo no boot |
| Um motor gira ao contrário | Inverta os dois fios só daquele motor |
| O rover vira para o lado errado | Troque os conectores MotorA e MotorB |
| O motor direito dá um tranco ao ligar | Normal: o TX do ESP envia dados durante o boot |
| O LED azul do ESP pisca enquanto anda | Normal: o LED fica no GPIO2, que controla o motor esquerdo |
| "Sem sinal" na página | O celular saiu da rede `ROVER - UFERSA`. Alguns Android trocam sozinhos para uma rede com internet |

## Estrutura do repositório

```
CodigoHooverESPNOW/
├── CodigoROVER.cpp            ← firmware atual (controle por HTTP)
└── CodigoROVERwebsockets.cpp  ← versão experimental com WebSocket (não mantida)
```

O nome da pasta é histórico: o projeto **não** usa ESP-NOW.
