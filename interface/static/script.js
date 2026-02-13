// Estado atual do gráfico de barras
let dadosAtuais = {
    labels: ['Temperatura (°C)', 'Umidade (%)', 'Ângulo (°)'],
    valores: [0, 0, 0]
};
function parseNum(val) {
    let n = parseFloat(val.replace(',', '.'));
    return isNaN(n) ? null : n;
}
const ctx = document.getElementById('chartHistorico').getContext('2d');
const chart = new Chart(ctx, {
    type: 'bar',
    data: {
        labels: dadosAtuais.labels,
        datasets: [
            {
                label: 'Valor Atual',
                data: dadosAtuais.valores,
                backgroundColor: [
                    'rgba(255,112,67,0.7)',
                    'rgba(66,165,245,0.7)',
                    'rgba(102,187,106,0.7)'
                ],
                borderColor: [
                    '#ff7043',
                    '#42a5f5',
                    '#66bb6a'
                ],
                borderWidth: 1
            }
        ]
    },
    options: {
        responsive: true,
        plugins: {
            legend: { display: false },
            title: { display: false },
            datalabels: {
                anchor: 'end',
                align: 'end',
                color: '#222',
                font: {
                    weight: 'bold',
                    size: 16
                },
                formatter: function(value) {
                    return value;
                }
            }
        },
        scales: {
            y: {
                beginAtZero: true
            }
        }
    },
    plugins: [ChartDataLabels]
});
function atualizarPainel(dados) {
    document.getElementById('temp').textContent = dados.temperatura;
    document.getElementById('umid').textContent = dados.umidade;
    document.getElementById('ang').textContent = dados.angulo;
    var alertaDiv = document.getElementById('alerta');
    alertaDiv.textContent = dados.alerta;
    alertaDiv.className = 'valor ' + (dados.alerta === 'ATIVO' ? 'alerta-ativo' : 'alerta-inativo');

    // Atualiza as barras com os valores mais recentes
    const temp = parseNum(dados.temperatura);
    const umid = parseNum(dados.umidade);
    const ang = parseNum(dados.angulo);
    if (temp !== null && umid !== null && ang !== null) {
        dadosAtuais.valores = [temp, umid, ang];
        chart.data.datasets[0].data = dadosAtuais.valores;
        chart.update();
    }
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

// Busca novos dados a cada 4 segundos
setInterval(buscarDados, 4000);
buscarDados();
