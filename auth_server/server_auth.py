#!/usr/bin/env python3
"""Simple auth server with secure password hashing and JWT.

Endpoints:
- POST /register {username, password}
- POST /login {username, password}

Usage (dev, self-signed TLS):
python server_auth.py --host 0.0.0.0 --port 5000 --ssl-cert cert.pem --ssl-key key.pem

For production TLS use a reverse proxy (Caddy) with automatic certs; see README.md.
"""
import argparse
import json
import sqlite3
import time
import os
from datetime import datetime, timedelta

from flask import Flask, request, jsonify
from passlib.hash import argon2
import jwt

try:
    from flask_limiter import Limiter
    from flask_limiter.util import get_remote_address
    HAS_LIMITER = True
except Exception:
    HAS_LIMITER = False

DB_PATH = os.path.join(os.path.dirname(__file__), 'accounts.db')
JWT_SECRET = os.environ.get('AUTH_JWT_SECRET', 'change_this_secret')
JWT_ALGO = 'HS256'
ACCESS_EXPIRE_MIN = 15

app = Flask(__name__)

if HAS_LIMITER:
    limiter = Limiter(app, key_func=get_remote_address, default_limits=["200 per day", "50 per hour"])
else:
    limiter = None


def init_db():
    need_perm_fix = not os.path.exists(DB_PATH)
    conn = sqlite3.connect(DB_PATH)
    cur = conn.cursor()
    cur.execute('''
    CREATE TABLE IF NOT EXISTS users (
        username TEXT PRIMARY KEY,
        password_hash TEXT NOT NULL,
        created_at INTEGER NOT NULL
    )
    ''')
    conn.commit()
    conn.close()
    if need_perm_fix:
        try:
            # Attempt to restrict file ACLs on POSIX systems; Windows ACLs handled in README.
            os.chmod(DB_PATH, 0o600)
        except Exception:
            pass


def create_token(username):
    now = datetime.utcnow()
    payload = {
        'sub': username,
        'iat': int(now.timestamp()),
        'exp': int((now + timedelta(minutes=ACCESS_EXPIRE_MIN)).timestamp())
    }
    token = jwt.encode(payload, JWT_SECRET, algorithm=JWT_ALGO)
    if isinstance(token, bytes):
        token = token.decode('utf-8')
    return token


@app.route('/register', methods=['POST'])
def register():
    if not request.is_json:
        return jsonify({'error': 'JSON required'}), 400
    data = request.get_json()
    username = data.get('username', '').strip()
    password = data.get('password', '')
    if not username or not password:
        return jsonify({'error': 'username and password required'}), 400
    if len(password) < 8:
        return jsonify({'error': 'password must be at least 8 characters'}), 400

    conn = sqlite3.connect(DB_PATH)
    cur = conn.cursor()
    cur.execute('SELECT username FROM users WHERE username=?', (username,))
    if cur.fetchone():
        conn.close()
        return jsonify({'error': 'user exists'}), 409

    pw_hash = argon2.hash(password)
    cur.execute('INSERT INTO users (username, password_hash, created_at) VALUES (?, ?, ?)',
                (username, pw_hash, int(time.time())))
    conn.commit()
    conn.close()
    token = create_token(username)
    return jsonify({'status': 'ok', 'token': token})


@app.route('/login', methods=['POST'])
def login():
    if not request.is_json:
        return jsonify({'error': 'JSON required'}), 400
    data = request.get_json()
    username = data.get('username', '').strip()
    password = data.get('password', '')
    if not username or not password:
        return jsonify({'error': 'username and password required'}), 400

    conn = sqlite3.connect(DB_PATH)
    cur = conn.cursor()
    cur.execute('SELECT password_hash FROM users WHERE username=?', (username,))
    row = cur.fetchone()
    conn.close()
    if not row:
        return jsonify({'error': 'invalid credentials'}), 401
    pw_hash = row[0]
    try:
        ok = argon2.verify(password, pw_hash)
    except Exception:
        ok = False
    if not ok:
        return jsonify({'error': 'invalid credentials'}), 401
    token = create_token(username)
    return jsonify({'status': 'ok', 'token': token})


@app.route('/health', methods=['GET'])
def health():
    return jsonify({'status': 'ok'})


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--host', default='127.0.0.1')
    parser.add_argument('--port', type=int, default=5000)
    parser.add_argument('--ssl-cert')
    parser.add_argument('--ssl-key')
    args = parser.parse_args()

    init_db()
    print('Starting auth server on', args.host, args.port)
    if args.ssl_cert and args.ssl_key:
        ssl_context = (args.ssl_cert, args.ssl_key)
        app.run(host=args.host, port=args.port, ssl_context=ssl_context)
    else:
        app.run(host=args.host, port=args.port)


if __name__ == '__main__':
    main()
