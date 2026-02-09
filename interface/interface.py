"""
Flask + MQTT – painel de temperatura
------------------------------------
Subscriber MQTT que atualiza uma página HTML simples.
Compatível com paho-mqtt 2.x e com modo debug do Flask.
"""

from flask import Flask, render_template_string
import threading
import paho.mqtt.client as mqtt
import os                         # ← novo (para detectar processo correto)

app = Flask(__name__)

BROKER   = "test.mosquitto.org"   # broker alterado para mosquito
PORT     = 1883                    # porta padrão MQTT
CLIENTID = "mosquito_monitor"      # client ID alterado para contexto mosquito

# Tópicos monitorados (mantidos conforme solicitado)
TOPICS = {
    "hospital/cama/temperatura": "temperatura",
    "hospital/cama/umidade": "umidade",

    "hospital/cama/angulo": "angulo",
    "hospital/cama01/alerta": "alerta"
}

from Crypto.Cipher import AES
from Crypto.Util.Padding import unpad

KEY = b'SEGURANCA1234567'
IV = b'INICIALIV1234567'
# Dados recebidos
def decrypt_aes_cbc_pkcs7(ciphertext: bytes) -> bytes:
    """
    Descriptografa dados usando AES CBC com PKCS7.
    ciphertext: bytes criptografados
    Retorna: bytes do texto original
    """
    cipher = AES.new(KEY, AES.MODE_CBC, IV)
    decrypted = cipher.decrypt(ciphertext)
    return unpad(decrypted, AES.block_size)
dados_cama = {
    "temperatura": "Aguardando...",
    "umidade": "Aguardando...",
    "angulo": "Aguardando...",
    "alerta": "Aguardando..."
}

# Armazena o último tempo de atualização de cada campo
import time
timeout_segundos = 5
ultimos_tempos = {k: 0 for k in dados_cama}

# ─── Callbacks MQTT ──────────────────────────────────────────────
def on_connect(client, userdata, flags, rc):
    if rc == 0:
        print("✅ Conectado ao broker")
        for topic in TOPICS:
            result = client.subscribe(topic, qos=1)
            print(f"📝 Subscrito ao tópico: {topic} - Result: {result}")
    else:
        print(f"Erro de conexão MQTT → rc={rc}")

def on_message(client, userdata, msg):

    try:
        # Descriptografa o payload recebido
        decrypted_bytes = decrypt_aes_cbc_pkcs7(msg.payload)
        payload = decrypted_bytes.decode('utf-8').strip()
        print(f"📨 [RECEBIDO] {msg.topic}: {payload}")
    except Exception as e:
        print(f"❌ Erro ao descriptografar payload do tópico {msg.topic}: {e}")
        payload = None

    chave = TOPICS.get(msg.topic)
    if chave and payload is not None:
        dados_cama[chave] = payload  # atualiza painel
        ultimos_tempos[chave] = time.time()
        print(f"✅ Dados atualizados: {chave} = {payload}")
    elif chave:
        print(f"⚠️ Dados não atualizados para {chave} devido a erro de descriptografia.")
    else:
        print(f"⚠️ Tópico não reconhecido: {msg.topic}")

def mqtt_thread():
    # especifica versão de callbacks → compatível com paho-mqtt ≥ 2.0
    client = mqtt.Client(
        client_id=CLIENTID,
        protocol=mqtt.MQTTv311,
    )
    client.on_connect = on_connect
    client.on_message = on_message

    try:
        client.connect(BROKER, PORT, keepalive=60)
    except Exception as exc:
        print(f"❌ Falha na conexão MQTT: {exc}")
        return

    client.loop_forever()

# ─── Página HTML ────────────────────────────────────────────────
HTML_TEMPLATE = """
<!DOCTYPE html>
<html>
<head>
    <title>Monitoramento Hospitalar</title>
    <style>
        body {font-family: Arial, sans-serif; background:#f7f7f7; color:#222;
              display:flex; flex-direction:column; justify-content:center;
              align-items:center; height:100vh; margin:0;}
        .title {font-size:2em; margin-bottom:30px; text-shadow:0 0 5px #ccc;}
        .painel {
            display: flex;
            gap: 30px;
            margin-bottom: 30px;
        }
        .card {
            background: #fff;
            border-radius: 16px;
            box-shadow: 0 2px 12px #ddd;
            padding: 30px 40px;
            min-width: 180px;
            text-align: center;
        }
        .valor {
            font-size: 2.5em;
            font-weight: bold;
            margin-bottom: 10px;
        }
        .label {
            font-size: 1.1em;
            color: #888;
        }
        .status-ok { color: #2e7d32; }
        .status-alerta { color: #d32f2f; }
        .alerta-ativo { color: #d32f2f; }
        .alerta-inativo { color: #1976d2; }
    </style>
</head>
<body>
    <div class="title">Monitoramento da Cama Hospitalar</div>
    <div class="painel">
        <div class="card">
            <div id="temp" class="valor">{{ temperatura }}</div>
            <div class="label">Temperatura (°C)</div>
        </div>
        <div class="card">
            <div id="umid" class="valor">{{ umidade }}</div>
            <div class="label">Umidade (%)</div>
        </div>
        <div class="card">
            <div id="ang" class="valor">{{ angulo }}</div>
            <div class="label">Ângulo da cama (°)</div>
        </div>
    </div>
    <div class="painel">
        <div class="card">
            <div id="alerta" class="valor {% if alerta == 'ATIVO' %}alerta-ativo{% else %}alerta-inativo{% endif %}">{{ alerta }}</div>
            <div class="label">Alerta</div>
        </div>
    </div>
    <script>
    function atualizarPainel(dados) {
        document.getElementById('temp').textContent = dados.temperatura;
        document.getElementById('umid').textContent = dados.umidade;
        document.getElementById('ang').textContent = dados.angulo;
        // Alerta
        var alertaDiv = document.getElementById('alerta');
        alertaDiv.textContent = dados.alerta;
        alertaDiv.className = 'valor ' + (dados.alerta === 'ATIVO' ? 'alerta-ativo' : 'alerta-inativo');
    }
    async function buscarDados() {
        try {
            const resp = await fetch('/api/dados');
            if (resp.ok) {
                const dados = await resp.json();
                atualizarPainel(dados);
            }
        } catch (e) {}
    }
    setInterval(buscarDados, 1500);
    </script>
</body>
</html>
"""


@app.route("/")
def index():
    return render_template_string(
        HTML_TEMPLATE,
        temperatura=dados_cama["temperatura"],
        umidade=dados_cama["umidade"],
        angulo=dados_cama["angulo"],
        alerta=dados_cama["alerta"]
    )

# Rota API para AJAX
from flask import jsonify
@app.route("/api/dados")
def api_dados():
    agora = time.time()
    resposta = {}
    for campo, valor in dados_cama.items():
        # Só mostra 'Aguardando...' se já recebeu dado antes e passou do timeout
        if ultimos_tempos[campo] > 0 and (agora - ultimos_tempos[campo] > timeout_segundos):
            resposta[campo] = "Aguardando..."
        else:
            resposta[campo] = valor
    return jsonify(resposta)

# ─── Bootstrap ──────────────────────────────────────────────────
if __name__ == "__main__":
    # No modo debug, Flask executa este bloco duas vezes:
    # • 1ª vez: processo "watchdog" (WERKZEUG_RUN_MAIN não existe)
    # • 2ª vez: processo real → WERKZEUG_RUN_MAIN == 'true'
    if os.environ.get("WERKZEUG_RUN_MAIN") == "true":
        threading.Thread(target=mqtt_thread, daemon=True).start()

    app.run(host="0.0.0.0", port=5000, debug=True)   # auto-reload ativo
