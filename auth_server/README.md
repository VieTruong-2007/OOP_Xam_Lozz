Auth server (Flask) for centralized registration/login

Files created:
- server_auth.py  : Flask server using Argon2 and JWT
- requirements.txt
- Caddyfile       : sample reverse proxy for production TLS

Quick start (virtualenv recommended):

1) Install Python packages:

```powershell
python -m pip install -r requirements.txt
```

2) Run (LAN / self-signed TLS):

- Create a self-signed cert (for testing on LAN):

```powershell
# OpenSSL required; on Windows, use Git Bash or install OpenSSL
openssl req -x509 -newkey rsa:4096 -nodes -keyout key.pem -out cert.pem -days 365 -subj "/CN=dev.local"
```

- Run server with TLS:

```powershell
python server_auth.py --host 0.0.0.0 --port 5000 --ssl-cert cert.pem --ssl-key key.pem
```

Clients in LAN can connect to https://<server-ip>:5000 (they may need to trust the self-signed cert).

3) Production TLS (recommended):

Use Caddy to provide automatic HTTPS via Let's Encrypt. Configure `Caddyfile` and run Caddy on the machine. Caddy will obtain certs for your domain and reverse-proxy to the Flask server on localhost:5000.

4) Firewall:

Open port 5000 on the server if you plan to allow inbound connections from clients (LAN or internet). For Windows Firewall example:

```powershell
# open TCP port 5000 for inbound
New-NetFirewallRule -DisplayName "AuthServer5000" -Direction Inbound -LocalPort 5000 -Protocol TCP -Action Allow
```

5) DB file security:

After `accounts.db` is created, restrict NTFS permissions so only Administrators and SYSTEM can access it:

```powershell
icacls "D:\C\accounts.db" /inheritance:r
icacls "D:\C\accounts.db" /grant:r Administrators:F "NT AUTHORITY\SYSTEM":F
```

6) Client integration:

Client (game) should POST JSON to `/register` and `/login` and use HTTPS. Example POST body:

```json
{"username": "alice", "password": "hunter2"}
```

Server returns JSON `{ "status": "ok", "token": "..." }` on success.

Security notes:
- Use a long random `AUTH_JWT_SECRET` environment variable in production.
- Prefer Caddy / real TLS over self-signed certs for internet access.
- Rate limit and monitoring recommended.
