# Netfrequentie Meting Lokaal

**LET OP**: Pas zowel *hsnetmeterlocal.py* als *hsnetmeterlocal.html* aan naar het juiste ip-adres. Dit is slechts een tijdelijke oplossing.

Om lokaal een netfrequentie meting te starten moeten twee python (v3) scripts gestart worden:

- python -m http.server 9000
- python hsnetmeterlocal.py

Bij gebruik van Raspbian zal *python* moeten worden vervangen door *python3*. Het gemakkelijkst is om via het commando *screen* een nieuwe terminal die op de achtergrond draait te starten en vanuit twee sessies de commando's te starten. Het is ook mogelijk om het commando op de achtergrond te starten met een '&' op het einde van de regel.

Open vervolgens een browser sessie en navigeer naar het adres van je raspberry pi met port 9000 erachter. Open vervolgens de html file genaamd 'hsnetmeterlocal.html'.
