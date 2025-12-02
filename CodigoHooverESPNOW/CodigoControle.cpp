// Codigo Atualizado Versão 1.0
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>

// --- Configuração da Rede ---
const char *ssid = "ROVER - UFERSA"; 
const char *password = "12345678"; 
ESP8266WebServer server(80);

// --- Configuração dos Pinos ---
#define PINO_IN1 0  // Motor Esq
#define PINO_IN2 2  // Motor Esq
#define PINO_IN3 1  // Motor Dir (TX)
#define PINO_IN4 3  // Motor Dir (RX)

// Velocidade Máxima (0 a 1023)
#define VELOCIDADE_MAX 1023 

// Variável para segurança (Watchdog)
unsigned long ultimoComando = 0;

// --- HTML + JS Atualizado para enviar comandos contínuos ---
const char *HTML_CONTROLE = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
    <title>Controle Hoover V4.1</title>
    <meta name="viewport" content="width=device-width, initial-scale=1, user-scalable=no, orientation=landscape">
    <style>
        * { box-sizing: border-box; margin: 0; padding: 0; user-select: none; -webkit-user-select: none; -webkit-touch-callout: none; }
        html, body { height: 100vh; width: 100vw; font-family: sans-serif; background-color: #1a2933; color: #e0e0e0; display: flex; flex-direction: column; justify-content: space-between; align-items: center; overflow: hidden; }
        h1 { color: #00bfff; margin-top: 10px; font-size: 1.5em; }
        #status { color: #a0a0a0; margin-bottom: 10px; font-size: 1em; }
        
        #main-control-area { display: flex; flex-direction: row; justify-content: space-between; align-items: center; width: 90%; flex-grow: 1; }
        .control-btn { width: 100px; height: 100px; font-size: 3.5em; background: #283a47; border: 3px solid #00bfff; border-radius: 15px; color: #00bfff; margin: 5px; touch-action: none; display: flex; justify-content: center; align-items: center; }
        .control-btn:active, .control-btn.active { background: #00bfff; color: white; transform: scale(0.95); }
        
        #direction-group { display: flex; flex-direction: row; }
        #throttle-group { display: flex; flex-direction: column; }
    </style>
</head>
<body>
    <h1>HOOVER V4.1</h1>
    <div id="main-control-area">
        <div id="direction-group">
            <button class="control-btn" id="btn-left">&#9664;</button>
            <button class="control-btn" id="btn-right">&#9654;</button>
        </div>
        <div id="throttle-group">
            <button class="control-btn" id="btn-up">&#9650;</button>
            <button class="control-btn" id="btn-down">&#9660;</button>
        </div>
    </div>
    <h3 id="status">Status: Parado</h3>

    <script>
        const statusEl = document.getElementById('status');
        let currentY = 0.0, currentX = 0.0;
        let commandInterval = null; // Para repetir o comando

        function sendCommand() {
            // Atualiza visual
            let txt = "Parado";
            if (currentY > 0) txt = "Frente"; else if (currentY < 0) txt = "Trás";
            if (currentX < 0) txt += " + Esq"; else if (currentX > 0) txt += " + Dir";
            statusEl.innerText = "Status: " + txt;

            // Envia para o ESP (ignora erros para não travar)
            fetch(`/joy?x=${currentX}&y=${currentY}`).catch(e => {});
        }

        // Função que inicia o envio repetitivo (Heartbeat)
        function startSending() {
            if (commandInterval) clearInterval(commandInterval);
            sendCommand(); // Envia imediatamente
            commandInterval = setInterval(sendCommand, 100); // Reenvia a cada 100ms
        }

        function stopSending() {
            if (commandInterval) clearInterval(commandInterval);
            currentX = 0.0; currentY = 0.0;
            sendCommand(); // Envia comando de parada
        }

        function bindButton(id, actionStart) {
            const btn = document.getElementById(id);
            const start = (e) => { e.preventDefault(); actionStart(); btn.classList.add('active'); startSending(); };
            const end = (e) => { e.preventDefault(); btn.classList.remove('active'); stopSending(); };

            btn.addEventListener('mousedown', start);
            btn.addEventListener('touchstart', start);
            
            btn.addEventListener('mouseup', end);
            btn.addEventListener('mouseleave', end);
            btn.addEventListener('touchend', end);
        }

        bindButton('btn-left',  () => { currentX = -1.0; });
        bindButton('btn-right', () => { currentX = 1.0; });
        bindButton('btn-up',    () => { currentY = 1.0; });
        bindButton('btn-down',  () => { currentY = -1.0; });

        // Segurança extra para touch
        document.body.addEventListener('touchend', (e) => {
            if(e.target.tagName !== 'BUTTON') { stopSending(); }
        });
    </script>
</body>
</html>
)rawliteral";


// --- LÓGICA DE CONTROLE (Igual à V4) ---
void controlarMotores(float x, float y) {
    int velocidadeFrente = y > 0 ? map(y * 100, 0, 100, 0, VELOCIDADE_MAX) : 0;
    int velocidadeTras = y < 0 ? map(y * -100, 0, 100, 0, VELOCIDADE_MAX) : 0;
    int velMotorEsquerdo = 0;
    int velMotorDireito = 0;

    if (y > 0) { // FRENTE
        if (x > 0) { velMotorEsquerdo = velocidadeFrente; velMotorDireito = velocidadeFrente * (1.0 - x); }
        else if (x < 0) { velMotorEsquerdo = velocidadeFrente * (1.0 + x); velMotorDireito = velocidadeFrente; }
        else { velMotorEsquerdo = velocidadeFrente; velMotorDireito = velocidadeFrente; }
    } else if (y < 0) { // TRÁS
        if (x > 0) { velMotorEsquerdo = velocidadeTras; velMotorDireito = velocidadeTras * (1.0 - x); }
        else if (x < 0) { velMotorEsquerdo = velocidadeTras * (1.0 + x); velMotorDireito = velocidadeTras; }
        else { velMotorEsquerdo = velocidadeTras; velMotorDireito = velocidadeTras; }
    } else { // PARADO - GIRO NO EIXO
        if (x > 0) { 
            int velGiro = (int)(VELOCIDADE_MAX * 0.8);
            analogWrite(PINO_IN2, 0); analogWrite(PINO_IN1, velGiro);
            analogWrite(PINO_IN3, 0); analogWrite(PINO_IN4, velGiro);
            return;
        } else if (x < 0) { 
            int velGiro = (int)(VELOCIDADE_MAX * 0.8);
            analogWrite(PINO_IN1, 0); analogWrite(PINO_IN2, velGiro);
            analogWrite(PINO_IN4, 0); analogWrite(PINO_IN3, velGiro);
            return;
        }
    }

    // Aplica aos pinos
    if (y > 0) { analogWrite(PINO_IN2, 0); analogWrite(PINO_IN1, velMotorEsquerdo); } 
    else if (y < 0) { analogWrite(PINO_IN1, 0); analogWrite(PINO_IN2, velMotorEsquerdo); } 
    else { analogWrite(PINO_IN1, 0); analogWrite(PINO_IN2, 0); }

    if (y > 0) { analogWrite(PINO_IN4, 0); analogWrite(PINO_IN3, velMotorDireito); } 
    else if (y < 0) { analogWrite(PINO_IN3, 0); analogWrite(PINO_IN4, velMotorDireito); } 
    else { analogWrite(PINO_IN3, 0); analogWrite(PINO_IN4, 0); }
}

void handleJoy() {
    ultimoComando = millis(); // <--- WATCHDOG: Reseta o cronômetro
    float x = 0; float y = 0;
    if (server.hasArg("x")) x = server.arg("x").toFloat();
    if (server.hasArg("y")) y = server.arg("y").toFloat();
    controlarMotores(x, y);
    server.send(200, "text/plain", "OK");
}

void handleRoot() { server.send(200, "text/html", HTML_CONTROLE); }
void handleNotFound() { server.send(404, "text/plain", "Nao encontrado"); }

void setup() {
    // Configura Pinos
    pinMode(PINO_IN1, OUTPUT); pinMode(PINO_IN2, OUTPUT);
    pinMode(PINO_IN3, OUTPUT); pinMode(PINO_IN4, OUTPUT);
    
    // Boot Seguro: Garante nível lógico baixo imediatamente
    digitalWrite(PINO_IN1, LOW); digitalWrite(PINO_IN2, LOW);
    digitalWrite(PINO_IN3, LOW); digitalWrite(PINO_IN4, LOW);

    analogWriteRange(VELOCIDADE_MAX);
    
    WiFi.softAP(ssid, password);
    
    server.on("/", HTTP_GET, handleRoot);
    server.on("/joy", HTTP_GET, handleJoy);
    server.onNotFound(handleNotFound);
    server.begin();
}

void loop() {
    server.handleClient();

    // --- WATCHDOG CHECK ---
    // Se passar 400ms sem receber sinal novo, para o carro.
    if (millis() - ultimoComando > 400) {
        controlarMotores(0.0, 0.0);
    }
}

