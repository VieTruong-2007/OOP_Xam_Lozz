from flask import Flask, render_template, request, redirect, url_for, send_from_directory, flash, session
import os
from werkzeug.utils import secure_filename
from datetime import datetime
import json

UPLOAD_FOLDER = os.path.join(os.path.dirname(__file__), 'uploads')
ALLOWED_EXTENSIONS = {'.zip', '.exe', '.msi', '.txt', '.md'}
ADMIN_PASSWORD = os.environ.get('WEB_ADMIN_PASSWORD', 'change_this_password')

app = Flask(__name__)
app.config['UPLOAD_FOLDER'] = UPLOAD_FOLDER
app.secret_key = os.environ.get('FLASK_SECRET_KEY', 'dev-secret-key')
ASSET_FOLDER = os.path.normpath(os.path.join(os.path.dirname(__file__), '..', 'assets'))

os.makedirs(UPLOAD_FOLDER, exist_ok=True)

METADATA_FILE = os.path.join(UPLOAD_FOLDER, 'metadata.json')


def load_metadata():
    if not os.path.exists(METADATA_FILE):
        return {}
    try:
        with open(METADATA_FILE, 'r', encoding='utf-8') as f:
            return json.load(f)
    except Exception:
        return {}


def save_metadata(meta):
    with open(METADATA_FILE, 'w', encoding='utf-8') as f:
        json.dump(meta, f, indent=2, ensure_ascii=False)


def format_file_size(size_bytes):
    if size_bytes < 1024:
        return f"{size_bytes} B"
    if size_bytes < 1024 ** 2:
        return f"{size_bytes / 1024:.1f} KB"
    if size_bytes < 1024 ** 3:
        return f"{size_bytes / 1024 ** 2:.1f} MB"
    return f"{size_bytes / 1024 ** 3:.1f} GB"


def format_upload_time(iso_string):
    if not iso_string:
        return 'Chưa xác định'
    try:
        dt = datetime.fromisoformat(iso_string.replace('Z', '+00:00'))
        return dt.strftime('%d/%m/%Y %H:%M')
    except ValueError:
        return iso_string


def allowed_file(filename):
    _, ext = os.path.splitext(filename)
    return ext.lower() in ALLOWED_EXTENSIONS


@app.route('/')
def index():
    files = []
    meta = load_metadata()
    for name in os.listdir(app.config['UPLOAD_FOLDER']):
        path = os.path.join(app.config['UPLOAD_FOLDER'], name)
        if os.path.isfile(path) and name != 'metadata.json':
            info = meta.get(name, {})
            uploaded_at = info.get('uploaded_at')
            files.append({
                'name': name,
                'size_text': format_file_size(os.path.getsize(path)),
                'uploaded_at': format_upload_time(uploaded_at),
                'uploaded_at_sort': uploaded_at or '',
                'notes': info.get('notes', 'Không có chú thích.')
            })
    files.sort(key=lambda x: x['uploaded_at_sort'], reverse=True)
    return render_template('index.html', files=files)


@app.route('/files/<path:filename>')
def files(filename):
    return send_from_directory(app.config['UPLOAD_FOLDER'], filename, as_attachment=True)


@app.route('/admin', methods=['GET', 'POST'])
def admin():
    if request.method == 'POST':
        # login or upload
        if 'login' in request.form:
            pwd = request.form.get('password', '')
            if pwd == ADMIN_PASSWORD:
                session['admin'] = True
                flash('Đăng nhập thành công', 'success')
                return redirect(url_for('admin'))
            else:
                flash('Mật khẩu không chính xác', 'danger')
                return redirect(url_for('admin'))
        if 'upload' in request.form:
            if not session.get('admin'):
                flash('Yêu cầu quyền admin', 'danger')
                return redirect(url_for('admin'))
            f = request.files.get('file')
            notes = request.form.get('notes', '')
            if not f:
                flash('Chưa chọn file', 'warning')
                return redirect(url_for('admin'))
            filename = secure_filename(f.filename)
            if not allowed_file(filename):
                flash('Loại file không được phép', 'warning')
                return redirect(url_for('admin'))
            dest = os.path.join(app.config['UPLOAD_FOLDER'], filename)
            f.save(dest)
            meta = load_metadata()
            meta[filename] = {
                'uploaded_at': datetime.utcnow().isoformat() + 'Z',
                'notes': notes
            }
            save_metadata(meta)
            flash('Tải lên thành công', 'success')
            return redirect(url_for('admin'))
    files = []
    if session.get('admin'):
        meta = load_metadata()
        for name in os.listdir(app.config['UPLOAD_FOLDER']):
            path = os.path.join(app.config['UPLOAD_FOLDER'], name)
            if os.path.isfile(path) and name != 'metadata.json':
                info = meta.get(name, {})
                uploaded_at = info.get('uploaded_at')
                files.append({
                    'name': name,
                    'size_text': format_file_size(os.path.getsize(path)),
                    'uploaded_at': format_upload_time(uploaded_at),
                    'notes': info.get('notes', 'Không có chú thích.')
                })
        files.sort(key=lambda x: x['uploaded_at'], reverse=True)
    return render_template('admin.html', admin=session.get('admin', False), files=files)


@app.route('/delete/<path:filename>', methods=['POST'])
def delete_file(filename):
    if not session.get('admin'):
        flash('Yêu cầu quyền admin', 'danger')
        return redirect(url_for('admin'))
    file_path = os.path.join(app.config['UPLOAD_FOLDER'], filename)
    if os.path.exists(file_path) and os.path.isfile(file_path):
        os.remove(file_path)
        meta = load_metadata()
        if filename in meta:
            meta.pop(filename)
            save_metadata(meta)
        flash(f'Đã xóa {filename}', 'success')
    else:
        flash('File không tồn tại', 'warning')
    return redirect(url_for('admin'))


@app.route('/logout')
def logout():
    session.pop('admin', None)
    flash('Logged out', 'info')
    return redirect(url_for('admin'))


@app.route('/assets/<path:filename>')
def serve_assets(filename):
    return send_from_directory(ASSET_FOLDER, filename)


if __name__ == '__main__':
    app.run(host='0.0.0.0', port=8080, debug=True)
