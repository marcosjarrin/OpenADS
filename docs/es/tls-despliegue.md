---
title: Despliegue TLS
layout: default
parent: Inicio (ES)
nav_order: 7
permalink: /es/tls-despliegue/
---

# Despliegue TLS para Studio

OpenADS Studio escucha hoy en **HTTP plano**
(`openads_serverd --http-port N`). El protocolo wire tiene su
propio URI `tls://` para TLS cliente-side (M12.12, mbedtls
3.6 LTS empaquetado), pero la consola HTTP embebida usa
`cpp-httplib`, que solo soporta TLS vía OpenSSL — no bundlamos
OpenSSL para mantener el binario ligero.

Para servir Studio sobre HTTPS en producción, termina TLS
delante del daemon. Tres opciones probadas; todas dejan
`openads_serverd` escuchando en `127.0.0.1` para que solo el
proxy sea accesible desde la red.

## Opción 1 — Caddy (recomendada)

Caddy auto-provisiona certificado Let's Encrypt, redirige
HTTP → HTTPS, configura TLS sanos, recarga config al cambio.
**Caddyfile**:

```caddyfile
studio.midominio.com {
    reverse_proxy 127.0.0.1:6263
}
```

```sh
sudo apt-get install caddy
sudo nano /etc/caddy/Caddyfile        # pega el snippet
sudo systemctl reload caddy
```

Studio ya accesible en `https://studio.midominio.com/`.
Cert renovado automáticamente; Caddy hace OCSP stapling.

## Opción 2 — nginx

```nginx
server {
    listen 443 ssl http2;
    server_name studio.midominio.com;

    ssl_certificate     /etc/letsencrypt/live/studio.midominio.com/fullchain.pem;
    ssl_certificate_key /etc/letsencrypt/live/studio.midominio.com/privkey.pem;

    location / {
        proxy_pass         http://127.0.0.1:6263;
        proxy_http_version 1.1;
        proxy_set_header   Host              $host;
        proxy_set_header   X-Real-IP         $remote_addr;
        proxy_set_header   X-Forwarded-For   $proxy_add_x_forwarded_for;
        proxy_set_header   X-Forwarded-Proto https;
    }
}

server {
    listen 80;
    server_name studio.midominio.com;
    return 301 https://$server_name$request_uri;
}
```

Combinar con `certbot --nginx -d studio.midominio.com` para
cert Let's Encrypt.

## Opción 3 — stunnel (solo terminación TLS)

Superficie menor que HTTP proxies completos — stunnel solo
envuelve socket plano en TLS.

```ini
# /etc/stunnel/openads.conf
[studio]
accept  = 0.0.0.0:443
connect = 127.0.0.1:6263
cert    = /etc/stunnel/openads-fullchain.pem
key     = /etc/stunnel/openads-key.pem
```

```sh
sudo apt-get install stunnel4
sudo systemctl enable --now stunnel4
```

## Opción 4 — Túnel SSH (zero-config, single user)

Cuando solo un admin necesita Studio y ya hay login SSH:

```sh
ssh -L 6263:localhost:6263 user@server.midominio.com
# abrir http://localhost:6263 en navegador
```

SSH cifra todo el tráfico HTTP; el daemon sigue escuchando
en `127.0.0.1:6263` dentro del host remoto.

## Postura recomendada

```
+--------+      HTTPS        +-------+   HTTP plano       +-----------------+
| Browser| ---------------→ | Caddy | -----------------→ | openads_serverd |
+--------+   TLS público     +-------+    127.0.0.1:6263  +-----------------+
                              ↑                                ↓
                          Let's Encrypt                    data dir
```

Flags daemon:

```sh
openads_serverd \
    --port 6262 \
    --http-port 6263 \
    --data /srv/openads/data \
    --http-user admin:password-fuerte
```

Combina TLS del proxy con **`--http-user`** — aunque se
comprometiera el TLS terminator, las credenciales siguen
siendo requeridas para cualquier API call.

## HTTPS nativo en serverd

Una opción CMake `OPENADS_WITH_OPENSSL=ON` está en roadmap.
Lincaría backend OpenSSL de cpp-httplib, aceptaría flags
`--tls-cert <pem> --tls-key <pem>`, y serverd serviría
HTTPS sin proxy. Estado: planeado para milestone futuro
`studio.web.x`; mientras tanto, terminar TLS en proxy.
