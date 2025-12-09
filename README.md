# Sistema de Monitoramento IoT para Hidroponia

Este projeto foi desenvolvido para monitorar, em tempo real, dois elementos essenciais de um sistema hidropônico: a vazão da água e a luminosidade do ambiente. As informações são lidas pelo ESP8266 e enviadas para o aplicativo Blynk, permitindo acompanhar tudo pelo celular e receber alertas caso algo saia do padrão.

## 📌 Objetivo do Projeto

Criar um sistema de baixo custo capaz de detectar falhas como:
- Parada da bomba de água
- Entupimento no circuito hidráulico
- Baixa luminosidade nas plantas

Com isso, o usuário consegue agir rapidamente para evitar prejuízos no cultivo.

## 💡 Como o sistema funciona

1. O sensor de fluxo YF-S201 mede a vazão da água, enviando pulsos convertidos em L/min.
2. O LDR mede a intensidade luminosa.
3. O ESP8266 lê os dados, envia para o Blynk e controla a automação (sombrite no modo automático ou manual).
4. O Arduino Uno é utilizado apenas como fonte de alimentação 5V para o ESP e sensores.
5. O Blynk exibe todos os dados e envia alertas em caso de falha.

## 📱 Recursos no Blynk

- Gauge com a vazão em L/min
- Indicador de luminosidade
- Controle automático/manual
- Notificações de falha ("vazão igual a zero")

## 📁 Estrutura do Repositório

**/horta-codigo/hidroponia.ino** → Código completo do ESP8266

**/documentacao/projetodeIOT.pdf** → Documento final do projeto

**README.md** → Você está aqui

## ▶️ Como executar

1. Abra o arquivo `.ino` no Arduino IDE
2. Instale as bibliotecas necessárias:
   - `ESP8266WiFi`
   - `BlynkSimpleEsp8266`
3. Insira suas credenciais:
   - Wi-Fi (SSID e senha)
   - Template ID e token do Blynk
4. Faça upload para o ESP8266
5. Configure o app Blynk conectando aos pinos virtuais:
   - **V1** → Vazão
   - **V2** → Luminosidade
   - **V4** → Modo manual
   - **V5** → Modo automático

## 👨‍🏫 Observação para o professor

Todos os arquivos do projeto podem ser acessados diretamente nos links do repositório, incluindo o relatório em PDF e o código-fonte. O objetivo é facilitar a visualização da implementação e da documentação oficial.

## 🔧 Hardware Utilizado

- ESP8266 (NodeMCU ou similar)
- Sensor de fluxo YF-S201
- LDR (Sensor de luminosidade)
- Arduino Uno (apenas como fonte 5V)
- Módulo relé (para controle do sombrite)
- Resistores e componentes eletrônicos básicos

## 📞 Suporte

Para dúvidas técnicas sobre o projeto, consulte a documentação completa ou entre em contato com a equipe de desenvolvimento.
