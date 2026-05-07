---
title: Implantação TLS
layout: default
parent: Início (PT)
nav_order: 7
permalink: /pt/tls-implantacao/
---

# Implantação TLS para Studio

OpenADS Studio hoje escuta em **HTTP texto puro**
(`openads_serverd --http-port N`). O protocolo wire tem seu
próprio URI `tls://` para TLS lado-cliente (M12.12, mbedtls
3.6 LTS empacotado), mas o console HTTP embutido usa
`cpp-httplib`, que só suporta TLS via OpenSSL — não
empacotado para manter o binário enxuto.

Para servir Studio sobre HTTPS em produção, termine TLS na
frente do daemon. Três opções comprovadas; todas deixam
`openads_serverd` escutando em `127.0.0.1` — só o proxy é
acessível da rede.

## Opção 1 — Caddy (recomendada)

Caddy auto-provisiona certificado Let's Encrypt, redireciona
HTTP → HTTPS, define TLS seguro, recarrega ao mudar config.
**Caddyfile**:

```caddyfile
studio.meudominio.com {
    reverse_proxy 127.0.0.1:6263
}
```

```sh
sudo apt-get install caddy
sudo nano /etc/caddy/Caddyfile        # cole o snippet
sudo systemctl reload caddy
```

Studio acessível em `https://studio.meudominio.com/`.
Cert renovado automaticamente; Caddy faz OCSP stapling.

## Opção 2 — nginx

```nginx
server {
    listen 443 ssl http2;
    server_name studio.meudominio.com;

    ssl_certificate     /etc/letsencrypt/live/studio.meudominio.com/fullchain.pem;
    ssl_certificate_key /etc/letsencrypt/live/studio.meudominio.com/privkey.pem;

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
    server_name studio.meudominio.com;
    return 301 https://$server_name$request_uri;
}
```

Combine com `certbot --nginx -d studio.meudominio.com` para
cert Let's Encrypt.

## Opção 3 — stunnel (só terminação TLS)

Superfície menor que HTTP proxies completos — stunnel só
envolve o socket texto-puro em TLS.

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

## Opção 4 — Túnel SSH (zero-config, single user)

Quando só um admin precisa do Studio e já existe login SSH:

```sh
ssh -L 6263:localhost:6263 user@servidor.meudominio.com
# abrir http://localhost:6263 no navegador
```

SSH criptografa todo o tráfego HTTP; o daemon continua
escutando em `127.0.0.1:6263` dentro do host remoto.

## Postura recomendada

```
+--------+      HTTPS        +-------+   HTTP texto puro  +-----------------+
| Browser| ---------------→ | Caddy | -----------------→ | openads_serverd |
+--------+   TLS público     +-------+    127.0.0.1:6263  +-----------------+
                              ↑                                ↓
                          Let's Encrypt                    data dir
```

Flags do daemon:

```sh
openads_serverd \
    --port 6262 \
    --http-port 6263 \
    --data /srv/openads/data \
    --http-user admin:senha-forte
```

Combine TLS do proxy com **`--http-user`** — mesmo se o TLS
terminator fosse comprometido, as credenciais ainda seriam
exigidas para qualquer chamada API.

## HTTPS nativo no serverd

Uma opção CMake `OPENADS_WITH_OPENSSL=ON` está no roadmap.
Linkaria backend OpenSSL do cpp-httplib, aceitaria flags
`--tls-cert <pem> --tls-key <pem>`, e serverd serviria
HTTPS sem proxy. Estado: planejado para milestone futuro
`studio.web.x`; enquanto isso, termine TLS num proxy.
