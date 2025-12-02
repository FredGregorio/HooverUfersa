//Codigo ROVER usando Websockets 1.0
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <WebSocketsServer.h> // <--- Biblioteca Nova Obrigatória!

// --- Configuração da Rede ---
const char *ssid = "ROVERWS - UFERSA"; 
const char *password = "12345678"; 

// --- Servidores ---
ESP8266WebServer server(80); // Serve a página HTML (Porta 80)
WebSocketsServer webSocket = WebSocketsServer(81); // Canal rápido de dados (Porta 81)

// --- Pinos e Configurações ---
#define PINO_IN1 0
#define PINO_IN2 2
#define PINO_IN3 1
#define PINO_IN4 3
#define VELOCIDADE_MAX 1023 

unsigned long ultimoComando = 0; // Watchdog

// --- HTML (Frontend) ---
const char *HTML_CONTROLE = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
    <title>Hoover V5 - WebSocket</title>
    <meta name="viewport" content="width=device-width, initial-scale=1, user-scalable=no, orientation=landscape">
    <style>
        * { box-sizing: border-box; margin: 0; padding: 0; user-select: none; -webkit-touch-callout: none; }
        body { height: 100vh; width: 100vw; background-color: #1a2933; color: #e0e0e0; display: flex; flex-direction: column; align-items: center; overflow: hidden; font-family: sans-serif; }
        
        #status-bar { width: 100%; padding: 10px; text-align: center; background: #111; color: #00bfff; }
        #status-text { font-weight: bold; }

        #main-control { display: flex; flex-direction: row; justify-content: space-between; align-items: center; width: 90%; flex-grow: 1; }
        
        .btn { width: 110px; height: 110px; font-size: 3em; background: #283a47; border: 3px solid #00bfff; border-radius: 15px; color: #00bfff; margin: 5px; display: flex; justify-content: center; align-items: center; transition: 0.1s; }
        .btn:active, .btn.active { background: #00bfff; color: white; transform: scale(0.95); }

        #group-dir { display: flex; gap: 10px; }
        #group-throt { display: flex; flex-direction: column; gap: 10px; }
    </style>
</head>
<body>
    <div id="status-bar"><span id="status-text">Desconectado...</span></div>

    <div id="main-control">
        <div id="group-dir">
            <div class="btn" id="btn-left">&#9664;</div>
            <div class="btn" id="btn-right">&#9654;</div>
        </div>
        <div id="group-throt">
            <div class="btn" id="btn-up">&#9650;</div>
            <div class="btn" id="btn-down">&#9660;</div>
        </div>
    </div>

    <script>
        var connection = new WebSocket('ws://' + location.hostname + ':81/', ['arduino']);
        
        // Variáveis de controle
        let cx = 0, cy = 0;
        let intervalId = null;

        connection.onopen = function () {
            document.getElementById('status-text').innerText = "Conectado (WebSocket)";
            document.getElementById('status-text').style.color = "#00ff00";
        };
        connection.onerror = function (error) {
            document.getElementById('status-text').innerText = "Erro no WebSocket";
            document.getElementById('status-text').style.color = "red";
        };
        connection.onclose = function () {
            document.getElementById('status-text').innerText = "Desconectado";
            document.getElementById('status-text').style.color = "red";
        };

        function sendData() {
            // Envia string simples: "x,y" (ex: "1.0,-1.0")
            if (connection.readyState === WebSocket.OPEN) {
                connection.send(cx + "," + cy);
            }
        }

        // Loop de envio (Heartbeat - 15x por segundo para resposta rapida)
        function startLoop() {
            if(!intervalId) {
                sendData(); // Envia o primeiro
                intervalId = setInterval(sendData, 70); 
            }
        }
        
        function stopLoop() {
            // Não paramos o loop imediatamente ao soltar um botão,
            // pois o outro pode estar apertado. O loop só para se x e y forem 0.
            if (cx === 0 && cy === 0) {
                clearInterval(intervalId);
                intervalId = null;
                sendData(); // Envia o zero final
            }
        }

        function handleBtn(id, axis, val) {
            const el = document.getElementById(id);
            
            const press = (e) => { 
                e.preventDefault(); 
                el.classList.add('active'); 
                if(axis === 'x') cx = val; else cy = val;
                startLoop();
            };
            
            const release = (e) => { 
                e.preventDefault(); 
                el.classList.remove('active'); 
                if(axis === 'x') cx = 0; else cy = 0;
                stopLoop();
            };

            el.addEventListener('mousedown', press);
            el.addEventListener('touchstart', press);
            el.addEventListener('mouseup', release);
            el.addEventListener('touchend', release);
            el.addEventListener('mouseleave', release);
        }

        handleBtn('btn-left', 'x', -1.0);
        handleBtn('btn-right', 'x', 1.0);
        handleBtn('btn-up', 'y', 1.0);
        handleBtn('btn-down', 'y', -1.0);
        
        // Segurança Global
        document.body.addEventListener('touchend', (e) => {
            if(e.target.className.indexOf('btn') === -1) {
               cx=0; cy=0; stopLoop();
               document.querySelectorAll('.btn').forEach(b => b.classList.remove('active'));
            }
        });

    </script>
</body>
</html>
)rawliteral";

// --- Controle dos Motores (Igual ao anterior) ---
void controlarMotores(float x, float y) {
    int velocidadeFrente = y > 0 ? map(y * 100, 0, 100, 0, VELOCIDADE_MAX) : 0;
    int velocidadeTras = y < 0 ? map(y * -100, 0, 100, 0, VELOCIDADE_MAX) : 0;
    
    int esq = 0, dir = 0;

    if (y > 0) { // FRENTE
        if (x > 0) { esq = velocidadeFrente; dir = velocidadeFrente * (1.0 - x); }
        else if (x < 0) { esq = velocidadeFrente * (1.0 + x); dir = velocidadeFrente; }
        else { esq = velocidadeFrente; dir = velocidadeFrente; }
    } else if (y < 0) { // TRAS
        if (x > 0) { esq = velocidadeTras; dir = velocidadeTras * (1.0 - x); }
        else if (x < 0) { esq = velocidadeTras * (1.0 + x); dir = velocidadeTras; }
        else { esq = velocidadeTras; dir = velocidadeTras; }
    } else { // GIRO EIXO
        if (x != 0) {
           int giro = VELOCIDADE_MAX * 0.8;
           if (x > 0) { analogWrite(PINO_IN1, giro); analogWrite(PINO_IN2, 0); analogWrite(PINO_IN3, 0); analogWrite(PINO_IN4, giro); }
           else { analogWrite(PINO_IN1, 0); analogWrite(PINO_IN2, giro); analogWrite(PINO_IN3, giro); analogWrite(PINO_IN4, 0); }
           return;
        }
    }

    if (y > 0) { analogWrite(PINO_IN2, 0); analogWrite(PINO_IN1, esq); analogWrite(PINO_IN4, 0); analogWrite(PINO_IN3, dir); }
    else if (y < 0) { analogWrite(PINO_IN1, 0); analogWrite(PINO_IN2, esq); analogWrite(PINO_IN3, 0); analogWrite(PINO_IN4, dir); }
    else { digitalWrite(PINO_IN1, 0); digitalWrite(PINO_IN2, 0); digitalWrite(PINO_IN3, 0); digitalWrite(PINO_IN4, 0); }
}

// --- Evento do WebSocket (Onde a mágica acontece) ---
void webSocketEvent(uint8_t num, WStype_t type, uint8_t * payload, size_t length) {
    if (type == WStype_TEXT) {
        // Recebemos texto! Ex: "1.0,-1.0"
        ultimoComando = millis(); // Reseta Watchdog
        
        String texto = (char *)payload;
        int virgula = texto.indexOf(',');
        if (virgula > 0) {
            String sX = texto.substring(0, virgula);
            String sY = texto.substring(virgula + 1);
            
            float valX = sX.toFloat();
            float valY = sY.toFloat();
            
            controlarMotores(valX, valY);
        }
    }
}

void setup() {
    pinMode(PINO_IN1, OUTPUT); digitalWrite(PINO_IN1, LOW);
    pinMode(PINO_IN2, OUTPUT); digitalWrite(PINO_IN2, LOW);
    pinMode(PINO_IN3, OUTPUT); digitalWrite(PINO_IN3, LOW);
    pinMode(PINO_IN4, OUTPUT); digitalWrite(PINO_IN4, LOW);
    analogWriteRange(VELOCIDADE_MAX);

    WiFi.softAP(ssid, password);

    // Inicia HTML
    server.on("/", []() { server.send(200, "text/html", HTML_CONTROLE); });
    server.begin();

    // Inicia WebSocket
    webSocket.begin();
    webSocket.onEvent(webSocketEvent); // Diz qual função processa os dados
}

void loop() {
    server.handleClient(); // Mantém HTML vivo
    webSocket.loop();      // Mantém WebSocket vivo

    // Watchdog (Segurança)
    if (millis() - ultimoComando > 400) {
        controlarMotores(0, 0);
    }
}
