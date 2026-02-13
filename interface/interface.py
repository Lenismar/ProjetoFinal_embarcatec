"""
Painel de monitoramento hospitalar com Flask e MQTT
--------------------------------------------------
Recebe dados via MQTT e exibe em uma página web simples.
Compatível com paho-mqtt 2.x. Feito para rodar em ambiente
de desenvolvimento (modo debug) do Flask.
"""

from flask import Flask, render_template, render_template_string
import threading
import paho.mqtt.client as mqtt
import os  # Usado para identificar o processo correto ao rodar em modo debug

app = Flask(__name__)

BROKER   = "test.mosquitto.org"   
PORT     = 1883                    
CLIENTID = "mosquito_monitor"      

# Tópicos MQTT monitorados
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
# Descriptografa payloads AES-CBC com padding PKCS7
def decrypt_aes_cbc_pkcs7(ciphertext: bytes) -> bytes:
    """
    Recebe bytes cifrados e retorna os bytes do texto claro.
    Usa a chave e IV definidos acima.
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

# Armazena o timestamp da última atualização de cada métrica
import time
timeout_segundos = 5
ultimos_tempos = {k: 0 for k in dados_cama}

# Handlers para conexão e recebimento de mensagens MQTT
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
        # tenta descriptografar o payload recebido
        decrypted_bytes = decrypt_aes_cbc_pkcs7(msg.payload)
        payload = decrypted_bytes.decode('utf-8').strip()
        print(f" [RECEBIDO] {msg.topic}: {payload}")
    except Exception as e:
        print(f" Erro ao descriptografar payload do tópico {msg.topic}: {e}")
        payload = None

    chave = TOPICS.get(msg.topic)
    if chave and payload is not None:
        # atualiza o estado exibido no painel
        dados_cama[chave] = payload
        ultimos_tempos[chave] = time.time()
        print(f" Dados atualizados: {chave} = {payload}")
    elif chave:
        print(f" Dados não atualizados para {chave} devido a erro de descriptografia.")
    else:
        print(f" Tópico não reconhecido: {msg.topic}")

def mqtt_thread():
    # Cria o cliente MQTT e configura os callbacks
    client = mqtt.Client(
        client_id=CLIENTID,
        protocol=mqtt.MQTTv311,
    )
    client.on_connect = on_connect
    client.on_message = on_message

    try:
        client.connect(BROKER, PORT, keepalive=60)
    except Exception as exc:
        print(f" Falha na conexão MQTT: {exc}")
        return

    client.loop_forever()




 # Rota principal que renderiza o painel com os últimos valores
@app.route("/")
def index():
    return render_template(
        "index.html",
        temperatura=dados_cama["temperatura"],
        umidade=dados_cama["umidade"],
        angulo=dados_cama["angulo"],
        alerta=dados_cama["alerta"]
    )

# API simples que retorna os dados atuais em JSON para o front-end
from flask import jsonify
@app.route("/api/dados")
def api_dados():
    return jsonify(dados_cama)

# Inicializa o app Flask e, no processo real do debug, inicia a thread MQTT
if __name__ == "__main__":
    # Em modo debug o Flask cria um processo de supervisão e um processo real.
    # Só queremos iniciar a thread MQTT no processo real.
    if os.environ.get("WERKZEUG_RUN_MAIN") == "true":
        threading.Thread(target=mqtt_thread, daemon=True).start()

    app.run(host="0.0.0.0", port=5000, debug=True)
