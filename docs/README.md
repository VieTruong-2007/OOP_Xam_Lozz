# Brick Game Project

## Cấu trúc dự án

- `src/` : mã nguồn chính của game (`src/C.cpp`).
- `auth_server/` : máy chủ xác thực đăng ký/đăng nhập, bao gồm `server_auth.py`, `requirements.txt` và `README.md`.
- `web_app/` : trang web tải game và admin upload.
- `dist/` : gói phân phối game và trang web tĩnh.
- `scripts/` : chứa các script xây dựng và hỗ trợ (build, watch, cài compiler).
- `assets/` : ảnh nền, theme và tài nguyên game.
- `data/` : dữ liệu tài khoản nội bộ (`data/accounts.txt`).
- `docs/` : tài liệu dự án và bảo mật.

## Cách sử dụng

### Chạy web tải game

1. Cài dependency:

```powershell
python -m pip install -r web_app\requirements.txt
```

2. Chạy server:

```powershell
python web_app\app.py
```

3. Mở:

- http://localhost:8080
- http://localhost:8080/admin

### Chạy auth server

1. Cài dependency:

```powershell
python -m pip install -r auth_server\requirements.txt
```

2. Chạy server:

```powershell
python auth_server\server_auth.py --host 0.0.0.0 --port 5000 --ssl-cert cert.pem --ssl-key key.pem
```

### Build game

1. Mở PowerShell hoặc CMD trong thư mục dự án.
2. Chạy script:

```powershell
scripts\build.bat
```

Hoặc dùng script tự động:

```powershell
scripts\WATCH-BUILD.bat
scripts\WATCH-BUILD-RUN.bat
```

### Lưu ý bảo mật

- Đặt `WEB_ADMIN_PASSWORD` và `FLASK_SECRET_KEY` khi chạy `web_app`.
- Với auth server, nên dùng HTTPS thực tế khi public.
- Duy trì `auth_server` và `web_app` riêng biệt để dễ quản lý.
