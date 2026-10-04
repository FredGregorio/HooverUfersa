// Codigo Atualizado Versão 1.1
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>

// --- Configuração da Rede ---
const char *ssid = "ROVER - UFERSA";
const char *password = "12345678";
ESP8266WebServer server(80);

// --- Configuração dos Pinos ---
// Na placa: TXD/RXD -> ENTRADAPH 1/2 (MotorA) e IO0/IO2 -> ENTRADAPH 3/4 (MotorB)
#define PINO_IN1 0  // Motor Esq (MotorB na placa)
#define PINO_IN2 2  // Motor Esq (MotorB na placa)
#define PINO_IN3 1  // Motor Dir (TX) (MotorA na placa)
#define PINO_IN4 3  // Motor Dir (RX) (MotorA na placa)

// Resolução do PWM (0 a 1023)
#define PWM_RANGE 1023

// Velocidade Máxima: motores N20 de 6V alimentados por bateria de 9V (via MX1508).
// Limita o PWM a 6/9 da faixa para a tensão média no motor não passar de ~6V.
#define TENSAO_BATERIA 9.0
#define TENSAO_MOTOR   6.0
#define VELOCIDADE_MAX ((int)(PWM_RANGE * TENSAO_MOTOR / TENSAO_BATERIA)) // ~682

// Variáveis para segurança (Watchdog)
unsigned long ultimoComando = 0;
bool motoresParados = true;

// --- Página de controle (HTML + CSS + JS) ---
// Fica na memória flash (PROGMEM) para não ocupar a RAM do ESP-01S.
// Enquanto um botão está apertado, a página envia GET /joy?x=..&y=..&v=.. a cada 100 ms.
const char HTML_CONTROLE[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="pt-BR">
<head>
<meta charset="utf-8">
<title>Hoover · Controle</title>
<meta name="viewport" content="width=device-width, initial-scale=1, maximum-scale=1, user-scalable=no, viewport-fit=cover">
<meta name="theme-color" content="#13202a">
<!-- iPhone: "Adicionar à Tela de Início" abre em tela cheia, sem as barras do Safari -->
<meta name="apple-mobile-web-app-capable" content="yes">
<meta name="mobile-web-app-capable" content="yes">
<meta name="apple-mobile-web-app-status-bar-style" content="black-translucent">
<meta name="apple-mobile-web-app-title" content="Hoover">
<style>
    :root {
        --bg: #13202a; --painel: #1c2c38; --botao: #243746;
        --texto: #e8f0f5; --suave: #9fb3c1;
        --destaque: #00bfff; --tinta: #03202d; --re: #ffb23e;
        --ok: #3ddc84; --erro: #ff6b5b;
        --btn: clamp(72px, min(30vmin, 25vw), 136px);
        --stick: clamp(150px, min(64vh, 40vw), 250px);
    }
    * { box-sizing: border-box; margin: 0; padding: 0; -webkit-tap-highlight-color: transparent; -webkit-user-select: none; user-select: none; -webkit-touch-callout: none; }
    html, body { height: 100%; overflow: hidden; overscroll-behavior: none; touch-action: none; }
    html { -webkit-text-size-adjust: 100%; text-size-adjust: 100%; } /* iPhone não aumenta o texto ao deitar */
    body {
        height: 100dvh; /* altura visível real, descontando as barras do navegador */
        display: grid; grid-template-rows: auto 1fr; grid-template-columns: minmax(0, 1fr); gap: 12px;
        padding: max(12px, env(safe-area-inset-top)) max(16px, env(safe-area-inset-right)) max(12px, env(safe-area-inset-bottom)) max(16px, env(safe-area-inset-left));
        background: var(--bg); color: var(--texto);
        font: 16px/1.3 system-ui, -apple-system, "Segoe UI", Roboto, sans-serif;
    }
    button { font: inherit; color: inherit; border: 0; background: none; cursor: pointer; }
    button:focus-visible { outline: 3px solid var(--destaque); outline-offset: 3px; }
    svg { display: block; fill: none; stroke: currentColor; stroke-width: 3; stroke-linecap: round; stroke-linejoin: round; }

    /* Barra superior: marca | modo de controle | sinal | tela cheia */
    header { display: flex; align-items: center; gap: 10px; }
    .marca { font-weight: 800; font-size: 1.125rem; letter-spacing: .04em; white-space: nowrap; }
    .marca span { margin-left: 6px; font-size: .8125rem; font-weight: 600; letter-spacing: .06em; color: var(--suave); }
    .seg { display: flex; padding: 4px; border-radius: 14px; background: var(--painel); }
    .seg button { min-width: 64px; height: 44px; padding: 0 12px; border-radius: 10px; color: var(--suave); font-weight: 600; transition: background-color .12s, color .12s; }
    .seg button[aria-checked="true"] { background: var(--botao); color: var(--destaque); }
    #modo { margin-right: auto; padding: 3px; border-radius: 13px; }
    #modo button { height: 34px; min-width: 0; display: flex; align-items: center; gap: 6px; font-size: .875rem; }
    #modo svg { width: 18px; height: 18px; stroke-width: 2; }
    #conexao { display: flex; align-items: center; gap: 8px; height: 40px; padding: 0 14px; border-radius: 20px; background: var(--painel); font-size: .875rem; color: var(--suave); font-variant-numeric: tabular-nums; white-space: nowrap; }
    #conexao[data-q="0"], #conexao[data-q="1"] { color: var(--texto); }
    .barras { display: flex; align-items: flex-end; gap: 2px; height: 14px; }
    .barras i { width: 3px; border-radius: 1px; background: var(--botao); transition: background-color .2s; }
    .barras i:nth-child(1) { height: 5px; }
    .barras i:nth-child(2) { height: 8px; }
    .barras i:nth-child(3) { height: 11px; }
    .barras i:nth-child(4) { height: 14px; }
    .barras i.on { background: var(--ok); }
    #conexao[data-q="2"] .barras i.on { background: var(--re); }
    #conexao[data-q="1"] .barras i.on { background: var(--erro); }
    #tela-cheia { width: 40px; height: 40px; border-radius: 12px; background: var(--painel); color: var(--suave); display: grid; place-items: center; }
    #tela-cheia svg { width: 20px; height: 20px; stroke-width: 2.2; }
    #tela-cheia[hidden] { display: none; }

    /* Área de controle: direção | painel | acelerador (ou joystick | painel) */
    main { display: grid; grid-template-columns: auto minmax(0, 1fr) auto; align-items: center; gap: 16px; min-height: 0; }
    #direcao { display: flex; gap: 12px; }
    #acelerador { display: flex; flex-direction: column; gap: 12px; }
    .ctl {
        width: var(--btn); height: var(--btn); border-radius: 22px;
        background: var(--botao); color: var(--destaque);
        display: grid; place-items: center; touch-action: none;
        box-shadow: 0 6px 16px rgba(0, 0, 0, .4);
        transition: transform .12s cubic-bezier(.2, .8, .2, 1), background-color .12s, color .12s, box-shadow .12s;
    }
    .ctl svg { width: 46%; height: 46%; }
    .ctl.ativo { background: var(--destaque); color: var(--tinta); transform: translateY(2px) scale(.96); box-shadow: 0 2px 6px rgba(0, 0, 0, .4); }
    #btn-down.ativo { background: var(--re); }
    #btn-left svg { transform: rotate(-90deg); }
    #btn-right svg { transform: rotate(90deg); }
    #btn-down svg { transform: rotate(180deg); }

    /* Joystick */
    #joystick { display: none; position: relative; width: var(--stick); height: var(--stick); border-radius: 50%; background: var(--painel); touch-action: none; }
    #joystick > svg { position: absolute; left: 50%; top: 50%; width: 18px; height: 18px; margin: -9px; color: var(--suave); stroke-width: 2.5; }
    #joystick .s-cima { transform: translateY(calc(var(--stick) * -.4)); }
    #joystick .s-baixo { transform: rotate(180deg) translateY(calc(var(--stick) * -.4)); }
    #joystick .s-esq { transform: rotate(-90deg) translateY(calc(var(--stick) * -.4)); }
    #joystick .s-dir { transform: rotate(90deg) translateY(calc(var(--stick) * -.4)); }
    .manete { position: absolute; left: 31%; top: 31%; width: 38%; height: 38%; border-radius: 50%; background: var(--botao); box-shadow: 0 6px 16px rgba(0, 0, 0, .4); transition: transform .18s cubic-bezier(.2, .8, .2, 1), background-color .12s; }
    #joystick.ativo .manete { background: var(--destaque); transition: background-color .12s; }
    #joystick.ativo.re .manete { background: var(--re); }
    body[data-modo="joystick"] #direcao, body[data-modo="joystick"] #acelerador { display: none; }
    body[data-modo="joystick"] #joystick { display: block; }
    body[data-modo="joystick"] main { grid-template-columns: auto minmax(0, 1fr); }

    /* Painel central: estado, rodas e velocidade */
    .painel { display: flex; flex-direction: column; align-items: center; gap: 12px; min-width: 0; text-align: center; }
    #estado { font-size: 1.25rem; font-weight: 700; }
    #dica { max-width: 30ch; font-size: .875rem; color: var(--suave); }
    #dica[hidden] { display: none; }
    .rover { display: flex; gap: 8px; height: clamp(84px, 30vh, 160px); }
    .roda { position: relative; width: 26px; border-radius: 9px; background: var(--painel); overflow: hidden; }
    .roda::after { content: ""; position: absolute; left: 5px; right: 5px; top: 50%; height: 2px; margin-top: -1px; background: var(--botao); }
    .roda i { position: absolute; left: 0; right: 0; top: 0; height: 50%; background: var(--destaque); transform: scaleY(0); transform-origin: bottom; transition: transform .12s ease-out; }
    .roda i.re { top: 50%; background: var(--re); transform-origin: top; }
    .corpo { width: clamp(56px, 13vh, 84px); border-radius: 16px; background: var(--painel); color: var(--suave); display: flex; justify-content: center; padding-top: 8px; }
    .corpo svg { width: 22px; height: 22px; stroke-width: 2.5; }
    .medidas { display: flex; gap: 20px; font-size: .8125rem; color: var(--suave); font-variant-numeric: tabular-nums; }
    .medidas b { display: inline-block; min-width: 4.5ch; text-align: left; color: var(--texto); font-weight: 600; }
    .velocidade { display: flex; align-items: center; gap: 10px; font-size: .875rem; color: var(--suave); }

    /* Celular em pé: painel em cima, controles embaixo */
    @media (orientation: portrait) {
        :root { --stick: min(76vw, 40vh, 260px); }
        main { grid-template-columns: minmax(0, 1fr) auto; grid-template-rows: minmax(0, 1fr) auto; align-items: end; }
        .painel { grid-row: 1; grid-column: 1 / -1; align-self: center; }
        .rover { height: clamp(72px, 20vh, 160px); }
        #direcao { grid-row: 2; grid-column: 1; }
        #acelerador { grid-row: 2; grid-column: 2; }
        body[data-modo="joystick"] main { grid-template-columns: minmax(0, 1fr); }
        body[data-modo="joystick"] #joystick { grid-row: 2; justify-self: center; }
    }
    /* Tela muito baixa (celular deitado com barra do navegador) */
    @media (orientation: landscape) and (max-height: 330px) {
        .medidas, .velocidade > span { display: none; }
    }
    /* Telas estreitas: esconde textos da barra superior, ficam ícones e números */
    @media (max-width: 700px) { .marca { display: none; } }
    @media (max-width: 420px) { .longo, #modo .rotulo { display: none; } }
    @media (prefers-reduced-motion: reduce) { * { transition: none !important; } }
</style>
</head>
<body data-modo="botoes">
<svg width="0" height="0" style="position:absolute" aria-hidden="true">
    <symbol id="seta" viewBox="0 0 24 24"><path d="M5 15.5l7-7 7 7"/></symbol>
</svg>

<header>
    <div class="marca">HOOVER<span>UFERSA</span></div>
    <div class="seg" id="modo" role="radiogroup" aria-label="Tipo de controle">
        <button role="radio" data-modo="botoes" aria-label="Botões">
            <svg viewBox="0 0 24 24"><path d="M9.5 3.5h5v6h6v5h-6v6h-5v-6h-6v-5h6z"/></svg><span class="rotulo">Botões</span>
        </button>
        <button role="radio" data-modo="joystick" aria-label="Joystick">
            <svg viewBox="0 0 24 24"><circle cx="12" cy="12" r="9"/><circle cx="12" cy="12" r="3.5" fill="currentColor"/></svg><span class="rotulo">Joystick</span>
        </button>
    </div>
    <div id="conexao">
        <span class="barras" aria-hidden="true"><i></i><i></i><i></i><i></i></span>
        <span id="sinal-texto">Conectando…</span>
    </div>
    <button id="tela-cheia" aria-label="Tela cheia" hidden>
        <svg viewBox="0 0 24 24"><path d="M4 9V4h5M20 9V4h-5M4 15v5h5M20 15v5h-5"/></svg>
    </button>
</header>

<main>
    <div id="direcao">
        <button class="ctl" id="btn-left" aria-label="Esquerda"><svg viewBox="0 0 24 24"><use href="#seta"/></svg></button>
        <button class="ctl" id="btn-right" aria-label="Direita"><svg viewBox="0 0 24 24"><use href="#seta"/></svg></button>
    </div>

    <div id="joystick" role="group" aria-label="Joystick: arraste para dirigir (no teclado, use as setas)">
        <svg class="s-cima" viewBox="0 0 24 24"><use href="#seta"/></svg>
        <svg class="s-baixo" viewBox="0 0 24 24"><use href="#seta"/></svg>
        <svg class="s-esq" viewBox="0 0 24 24"><use href="#seta"/></svg>
        <svg class="s-dir" viewBox="0 0 24 24"><use href="#seta"/></svg>
        <div class="manete"></div>
    </div>

    <section class="painel">
        <p id="estado" aria-live="polite">Parado</p>
        <p id="dica" role="alert" hidden></p>
        <div class="rover" aria-hidden="true">
            <div class="roda"><i id="roda-esq"></i></div>
            <div class="corpo"><svg viewBox="0 0 24 24"><use href="#seta"/></svg></div>
            <div class="roda"><i id="roda-dir"></i></div>
        </div>
        <div class="medidas"><span>Motor esq. <b id="pct-esq">0%</b></span><span>Motor dir. <b id="pct-dir">0%</b></span></div>
        <div class="velocidade">
            <span id="rotulo-vel">Velocidade</span>
            <div class="seg" id="vel" role="radiogroup" aria-labelledby="rotulo-vel">
                <button role="radio" data-v="0.4">Lenta</button>
                <button role="radio" data-v="0.7">Média</button>
                <button role="radio" data-v="1">Máx.</button>
            </div>
        </div>
    </section>

    <div id="acelerador">
        <button class="ctl" id="btn-up" aria-label="Frente"><svg viewBox="0 0 24 24"><use href="#seta"/></svg></button>
        <button class="ctl" id="btn-down" aria-label="Ré"><svg viewBox="0 0 24 24"><use href="#seta"/></svg></button>
    </div>
</main>

<script>
    const $ = (id) => document.getElementById(id);
    const BOTOES = { 'btn-left': ['x', -1], 'btn-right': ['x', 1], 'btn-up': ['y', 1], 'btn-down': ['y', -1] };
    const TECLAS = { ArrowLeft: 'btn-left', a: 'btn-left', ArrowRight: 'btn-right', d: 'btn-right',
                     ArrowUp: 'btn-up', w: 'btn-up', ArrowDown: 'btn-down', s: 'btn-down' };
    const ZONA_MORTA = 0.15; // joystick: centro que conta como "parado"
    const NOMES_SINAL = ['Sem sinal', 'No limite', 'Sinal fraco', 'Sinal bom', 'Sinal ótimo'];

    let segurados = [];  // botões apertados, na ordem em que foram apertados
    let joy = null;      // { x, y } enquanto o joystick está sendo usado
    let dedoJoystick = null;
    let vel = 1;         // fator de velocidade escolhido (0.4, 0.7 ou 1)
    let emVoo = 0, ultimoEnvio = 0, falhas = 0, ultimaAtualizacao = 0;
    const amostras = []; // respostas do ESP nos últimos 3 s, usadas no indicador de alcance

    // Valor de um eixo nos botões: vale o botão desse eixo apertado por último.
    // Assim soltar um botão nunca zera o outro eixo.
    function eixo(nome) {
        for (let i = segurados.length - 1; i >= 0; i--) {
            const [e, v] = BOTOES[segurados[i]];
            if (e === nome) return v;
        }
        return 0;
    }

    // Comando atual: o joystick (se estiver em uso) ou os botões / teclado
    function comando() { return joy || { x: eixo('x'), y: eixo('y') }; }
    function andando() { const c = comando(); return c.x !== 0 || c.y !== 0; }

    function apertar(id) {
        if (segurados.includes(id)) return;
        segurados.push(id);
        $(id).classList.add('ativo');
        if (navigator.vibrate) navigator.vibrate(10);
        mudou();
    }

    function soltar(id) {
        const i = segurados.indexOf(id);
        if (i < 0) return;
        segurados.splice(i, 1);
        $(id).classList.remove('ativo');
        mudou();
    }

    // Segurança: solta tudo se o celular trocar de app, apagar a tela etc.
    function pararTudo() {
        if (!segurados.length && !joy && dedoJoystick === null) return;
        segurados.forEach((id) => $(id).classList.remove('ativo'));
        segurados = [];
        soltarJoystick();
        mudou();
    }

    function mudou() { atualizarTela(); enviar(); }

    // --- Comunicação com o ESP ---
    function enviar() {
        const { x, y } = comando();
        const ativo = x !== 0 || y !== 0;
        emVoo++;
        ultimoEnvio = Date.now();
        const inicio = performance.now();
        const ctrl = new AbortController();
        const limite = setTimeout(() => ctrl.abort(), 700);
        fetch(`/joy?x=${x}&y=${y}&v=${vel}`, { cache: 'no-store', signal: ctrl.signal })
            .then((r) => { if (!r.ok) throw r.status; resposta(performance.now() - inicio, ativo); })
            .catch(() => resposta(null, ativo))
            .finally(() => { clearTimeout(limite); emVoo--; });
    }

    // Heartbeat: andando, reenvia a cada 100 ms (o ESP para sozinho após 400 ms sem comando).
    // Parado, manda um comando de parada por segundo só para saber se a conexão está viva.
    setInterval(() => {
        if (emVoo) return; // não empilha requisições se o ESP estiver demorando
        if (andando() || Date.now() - ultimoEnvio > 1000) enviar();
    }, 100);

    // --- Indicador de alcance ---
    // Perto do limite do alcance o Wi-Fi começa a repetir pacotes: o tempo de resposta
    // sobe e alguns comandos se perdem. Com o rover parado o celular "cochila" o Wi-Fi
    // e a resposta demora mesmo de perto, por isso o tempo só conta enquanto se dirige.
    function resposta(ms, ativo) {
        const agora = performance.now();
        amostras.push({ ok: ms !== null, ms, ativo, t: agora });
        // Só os últimos 3 s contam: ao voltar para perto, o indicador se recupera logo
        while (amostras.length > 12 || agora - amostras[0].t > 3000) amostras.shift();
        falhas = ms === null ? falhas + 1 : 0;
        mostrarSinal();
    }

    // Tempo de resposta típico (mediana) das respostas recentes
    function latencia() {
        const t = amostras.filter((a) => a.ok).map((a) => a.ms).sort((a, b) => a - b);
        return t.length ? t[t.length >> 1] : 0;
    }

    // 0 (sem sinal) a 4 (ótimo)
    function qualidade() {
        if (falhas >= 2) return 0;
        const perdas = amostras.filter((a) => !a.ok).length / amostras.length;
        const tempos = amostras.filter((a) => a.ok && a.ativo).map((a) => a.ms).sort((a, b) => a - b);
        const lento = tempos.length >= 4 ? tempos[Math.floor(tempos.length * 0.8)] : 0; // ignora picos isolados
        if (perdas >= 0.25 || lento > 400) return 1;
        if (perdas >= 0.1 || lento > 200) return 2;
        if (perdas > 0 || lento > 100) return 3;
        return 4;
    }

    const barras = document.querySelectorAll('.barras i');
    function mostrarSinal() {
        const q = qualidade();
        const el = $('conexao');
        const mudouQ = String(q) !== el.dataset.q;
        // A qualidade muda na hora; o número só 2x por segundo (para dar para ler)
        if (!mudouQ && performance.now() - ultimaAtualizacao < 500) return;
        ultimaAtualizacao = performance.now();
        if (mudouQ && q === 1 && andando() && navigator.vibrate) navigator.vibrate([80, 60, 80]);
        el.dataset.q = q;
        barras.forEach((b, i) => b.classList.toggle('on', i < q));
        $('sinal-texto').innerHTML = q === 0 ? 'Sem sinal'
            : `<span class="longo">${NOMES_SINAL[q]} · </span>${Math.round(latencia())} ms`;
        const dica = $('dica');
        dica.hidden = q > 1;
        dica.textContent = q === 0
            ? 'Sem resposta do rover. Confira se o celular ainda está no Wi-Fi do rover.'
            : 'Sinal no limite: aproxime-se do rover antes de perder o controle.';
    }

    // --- Tela ---
    // Mesma lógica de controlarMotores() no ESP, com cada roda de -1 (ré) a 1 (frente)
    function rodas(x, y) {
        const giro = x * 0.8 * (1 - Math.abs(y));
        const esq = (x < 0 ? y * (1 + x) : y) + giro;
        const dir = (x > 0 ? y * (1 - x) : y) - giro;
        return [esq, dir];
    }

    function textoEstado(x, y) {
        if (!x && !y) return 'Parado';
        if (!y) return x < 0 ? 'Girando para a esquerda' : 'Girando para a direita';
        return (y > 0 ? 'Frente' : 'Ré') + (x ? (x < 0 ? ' · curva à esquerda' : ' · curva à direita') : '');
    }

    function mostrarRoda(nome, valor) {
        const barra = $('roda-' + nome);
        barra.style.transform = `scaleY(${Math.abs(valor)})`;
        barra.classList.toggle('re', valor < 0);
        $('pct-' + nome).textContent = (valor < 0 ? '−' : '') + Math.round(Math.abs(valor) * 100) + '%';
    }

    function atualizarTela() {
        const { x, y } = comando();
        const [esq, dir] = rodas(x, y);
        mostrarRoda('esq', esq * vel);
        mostrarRoda('dir', dir * vel);
        $('estado').textContent = textoEstado(x, y);
    }

    // --- Botões (Pointer Events: funciona com toque, mouse e vários dedos) ---
    for (const id in BOTOES) {
        const b = $(id);
        b.addEventListener('pointerdown', (e) => {
            e.preventDefault();
            try { b.setPointerCapture(e.pointerId); } catch (_) {} // dedo escorregou para fora: continua apertado
            apertar(id);
        });
        for (const tipo of ['pointerup', 'pointercancel', 'lostpointercapture']) b.addEventListener(tipo, () => soltar(id));
        b.addEventListener('contextmenu', (e) => e.preventDefault());
    }

    // --- Joystick ---
    const stick = $('joystick'), manete = stick.querySelector('.manete');

    function moverJoystick(e) {
        const r = stick.getBoundingClientRect();
        const raio = r.width / 2, curso = raio * 0.62; // até onde a manete vai (sem sair da base)
        let dx = e.clientX - r.left - raio, dy = e.clientY - r.top - raio;
        const dist = Math.hypot(dx, dy);
        if (dist > curso) { dx *= curso / dist; dy *= curso / dist; }
        manete.style.transform = `translate(${dx}px, ${dy}px)`;

        let x = dx / curso, y = -dy / curso;
        const m = Math.hypot(x, y);
        if (m < ZONA_MORTA) { x = 0; y = 0; }
        else { const k = (m - ZONA_MORTA) / (1 - ZONA_MORTA) / m; x *= k; y *= k; } // recomeça do zero fora da zona morta
        if (Math.abs(x) < 0.12) x = 0; // "ímã" nos eixos: fácil andar reto ou girar no lugar
        if (Math.abs(y) < 0.12) y = 0;
        joy = { x: Math.round(x * 100) / 100, y: Math.round(y * 100) / 100 };
        stick.classList.toggle('re', joy.y < 0);
        atualizarTela();
        if (!emVoo) enviar(); // o heartbeat cuida do resto, sem empilhar requisições
    }

    function soltarJoystick() {
        dedoJoystick = null;
        joy = null;
        stick.classList.remove('ativo', 're');
        manete.style.transform = '';
    }

    stick.addEventListener('pointerdown', (e) => {
        e.preventDefault();
        if (dedoJoystick !== null) return;
        dedoJoystick = e.pointerId;
        try { stick.setPointerCapture(e.pointerId); } catch (_) {}
        stick.classList.add('ativo');
        if (navigator.vibrate) navigator.vibrate(10);
        moverJoystick(e);
    });
    stick.addEventListener('pointermove', (e) => { if (e.pointerId === dedoJoystick) moverJoystick(e); });
    for (const tipo of ['pointerup', 'pointercancel', 'lostpointercapture']) {
        stick.addEventListener(tipo, (e) => { if (e.pointerId === dedoJoystick) { soltarJoystick(); mudou(); } });
    }
    stick.addEventListener('contextmenu', (e) => e.preventDefault());

    // Teclado (para testar no computador): setas ou W A S D, nos dois modos
    const tecla = (e) => TECLAS[e.key.length === 1 ? e.key.toLowerCase() : e.key];
    addEventListener('keydown', (e) => { const id = tecla(e); if (id) { e.preventDefault(); if (!e.repeat) apertar(id); } });
    addEventListener('keyup', (e) => { const id = tecla(e); if (id) soltar(id); });

    document.addEventListener('visibilitychange', () => { if (document.hidden) pararTudo(); });
    addEventListener('blur', pararTudo);
    addEventListener('pagehide', pararTudo);

    // --- Preferências (lembradas no celular entre uma visita e outra) ---
    function lembrar(chave, valor) { try { localStorage.setItem(chave, valor); } catch (e) {} }
    function lembrado(chave) { try { return localStorage.getItem(chave); } catch (e) { return null; } }

    const opcoesVel = document.querySelectorAll('#vel button');
    function escolherVel(v) {
        vel = v;
        opcoesVel.forEach((b) => b.setAttribute('aria-checked', +b.dataset.v === v));
        lembrar('vel', v);
    }
    opcoesVel.forEach((b) => b.addEventListener('click', () => { escolherVel(+b.dataset.v); mudou(); }));
    const velSalva = +lembrado('vel');
    escolherVel([0.4, 0.7, 1].includes(velSalva) ? velSalva : 1);

    const opcoesModo = document.querySelectorAll('#modo button');
    function escolherModo(modo) {
        pararTudo();
        document.body.dataset.modo = modo;
        opcoesModo.forEach((b) => b.setAttribute('aria-checked', b.dataset.modo === modo));
        lembrar('modo', modo);
    }
    opcoesModo.forEach((b) => b.addEventListener('click', () => escolherModo(b.dataset.modo)));
    escolherModo(lembrado('modo') === 'joystick' ? 'joystick' : 'botoes');

    // --- Tela cheia (some com a barra do navegador no Android) ---
    const telaCheia = $('tela-cheia');
    if (document.documentElement.requestFullscreen) {
        telaCheia.hidden = false;
        telaCheia.addEventListener('click', () => {
            if (document.fullscreenElement) { document.exitFullscreen(); return; }
            document.documentElement.requestFullscreen({ navigationUI: 'hide' })
                .then(() => screen.orientation && screen.orientation.lock && screen.orientation.lock('landscape'))
                .catch(() => {});
        });
    }

    atualizarTela();
    enviar();
</script>
</body>
</html>
)rawliteral";


// --- LÓGICA DE CONTROLE ---
// x: -1 (esquerda) a 1 (direita) | y: -1 (ré) a 1 (frente)
// v: fator de velocidade escolhido na página (0.2 a 1), aplicado sobre VELOCIDADE_MAX
// Funciona com os botões (x e y valem -1, 0 ou 1) e com o joystick (valores intermediários).
#define FATOR_GIRO 0.8 // força do giro no próprio eixo (com y = 0)

// potencia: -1 (ré total) a 1 (frente total)
void acionarMotor(int pinoFrente, int pinoTras, float potencia, int velMax) {
    int pwm = fabs(potencia) * velMax;
    if (potencia > 0) { analogWrite(pinoTras, 0); analogWrite(pinoFrente, pwm); }
    else if (potencia < 0) { analogWrite(pinoFrente, 0); analogWrite(pinoTras, pwm); }
    else { analogWrite(pinoFrente, 0); analogWrite(pinoTras, 0); }
}

void controlarMotores(float x, float y, float v) {
    // Curva: a roda do lado para onde se vira desacelera (como um carro, inclusive na ré)
    float esq = (x < 0) ? y * (1.0 + x) : y;
    float dir = (x > 0) ? y * (1.0 - x) : y;

    // Giro no próprio eixo: máximo com y = 0 e some aos poucos conforme acelera.
    // Assim o joystick passa de "girar" para "curva" sem trancos.
    float giro = x * FATOR_GIRO * (1.0 - fabs(y));
    esq += giro;
    dir -= giro;

    int velMax = VELOCIDADE_MAX * v;
    acionarMotor(PINO_IN1, PINO_IN2, esq, velMax); // Motor Esq
    acionarMotor(PINO_IN3, PINO_IN4, dir, velMax); // Motor Dir
}

void handleJoy() {
    ultimoComando = millis(); // <--- WATCHDOG: Reseta o cronômetro
    motoresParados = false;
    float x = 0; float y = 0; float v = 1.0;
    if (server.hasArg("x")) x = constrain(server.arg("x").toFloat(), -1.0, 1.0);
    if (server.hasArg("y")) y = constrain(server.arg("y").toFloat(), -1.0, 1.0);
    if (server.hasArg("v")) v = constrain(server.arg("v").toFloat(), 0.2, 1.0);
    controlarMotores(x, y, v);
    server.send(200, "text/plain", "OK");
}

void handleRoot() { server.send_P(200, "text/html", HTML_CONTROLE); }
void handleNotFound() { server.send(404, "text/plain", "Nao encontrado"); }

// Diagnóstico: abra http://192.168.4.1/status para ver a memória do ESP.
// Se a memória livre cair sem parar enquanto dirige, há vazamento.
void handleStatus() {
    String s = "Memoria livre (heap): " + String(ESP.getFreeHeap()) + " bytes\n";
    s += "Maior bloco livre: " + String(ESP.getMaxFreeBlockSize()) + " bytes\n";
    s += "Fragmentacao: " + String(ESP.getHeapFragmentation()) + "%\n";
    s += "Programa: " + String(ESP.getSketchSize()) + " bytes (livre na flash: " + String(ESP.getFreeSketchSpace()) + " bytes)\n";
    s += "Celulares conectados: " + String(WiFi.softAPgetStationNum()) + "\n";
    s += "Ligado ha: " + String(millis() / 1000) + " s\n";
    server.send(200, "text/plain", s);
}

void setup() {
    // Configura Pinos
    pinMode(PINO_IN1, OUTPUT); pinMode(PINO_IN2, OUTPUT);
    pinMode(PINO_IN3, OUTPUT); pinMode(PINO_IN4, OUTPUT);

    // Boot Seguro: Garante nível lógico baixo imediatamente
    digitalWrite(PINO_IN1, LOW); digitalWrite(PINO_IN2, LOW);
    digitalWrite(PINO_IN3, LOW); digitalWrite(PINO_IN4, LOW);

    analogWriteRange(PWM_RANGE);

    // Só Access Point: sem o modo estação tentando conectar em redes antigas salvas
    // (isso faz o rádio trocar de canal e derruba o celular). Não grava config na flash.
    WiFi.persistent(false);
    WiFi.mode(WIFI_AP);
    WiFi.softAP(ssid, password);

    server.on("/", HTTP_GET, handleRoot);
    server.on("/joy", HTTP_GET, handleJoy);
    server.on("/status", HTTP_GET, handleStatus);
    server.onNotFound(handleNotFound);
    server.begin();
}

void loop() {
    server.handleClient();

    // --- WATCHDOG CHECK ---
    // Se passar 400ms sem receber sinal novo, para o carro (uma vez só, sem
    // reescrever o PWM milhares de vezes por segundo).
    if (!motoresParados && millis() - ultimoComando > 400) {
        controlarMotores(0.0, 0.0, 1.0);
        motoresParados = true;
    }
}
