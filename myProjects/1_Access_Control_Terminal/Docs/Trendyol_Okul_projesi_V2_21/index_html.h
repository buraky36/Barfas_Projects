const char INDEX_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>Offline Door System</title>
    <style>
        :root {
            --primary: #FF6B00;
            --bg: #121212;
            --card: #1E1E1E;
            --text: #E0E0E0;
            --text-muted: #A0A0A0;
            --success: #00C853;
            --danger: #FF3D00;
        }
        body {
            font-family: 'Segoe UI', Roboto, Helvetica, Arial, sans-serif;
            background-color: var(--bg);
            color: var(--text);
            margin: 0;
            display: flex;
            justify-content: center;
            align-items: center;
            min-height: 100vh;
        }
        .container {
            width: 100%;
            max-width: 400px;
            padding: 20px;
        }
        .card {
            background-color: var(--card);
            border-radius: 12px;
            padding: 24px;
            box-shadow: 0 4px 20px rgba(0,0,0,0.5);
            margin-bottom: 20px;
            text-align: center;
        }
        h2 { margin-top: 0; color: var(--primary); }
        h3 { border-bottom: 1px solid #444; padding-bottom: 10px; margin-top: 20px; color: var(--text-muted); font-size: 1.1em; }
        h4 { margin: 10px 0 5px; color: var(--text-muted); font-size: 0.9em; text-align: left; }
        
        input {
            width: 100%;
            padding: 12px;
            margin: 5px 0 10px;
            background: #2C2C2C;
            border: 1px solid #444;
            border-radius: 8px;
            color: white;
            box-sizing: border-box;
        }
        button {
            width: 100%;
            padding: 12px;
            background-color: var(--primary);
            color: white;
            border: none;
            border-radius: 8px;
            font-weight: bold;
            cursor: pointer;
            transition: opacity 0.2s;
            margin-top: 5px;
            margin-bottom: 5px;
        }
        button:active { opacity: 0.8; }
        button.btn-success { background-color: var(--success); }
        button.btn-danger { background-color: var(--danger); }
        button.btn-secondary { background-color: #444; }
        button.btn-door { background-color: #2980b9; } 
        
        .hidden { display: none; }
        .stat-row {
            display: flex;
            justify-content: space-between;
            margin: 10px 0;
            border-bottom: 1px solid #333;
            padding-bottom: 10px;
        }
        .label { color: var(--text-muted); }
        .value { font-weight: bold; }
        #toast {
            position: fixed;
            bottom: 20px;
            left: 50%;
            transform: translateX(-50%);
            background: #333;
            padding: 10px 20px;
            border-radius: 50px;
            display: none;
            z-index: 100;
        }
    </style>
</head>
<body>

    <div class="container">
        <!-- LOGIN SECTION -->
        <div id="login-section" class="card">
            <h2>Giriş Yap</h2>
            <p style="color:var(--text-muted)">Yönetici paneline erişmek için giriş yapın.</p>
            <input type="text" id="login-username" placeholder="Kullanıcı Adı">
            <input type="password" id="login-password" placeholder="Şifre">
            <button onclick="login()">Giriş</button>
        </div>

        <!-- DASHBOARD SECTION -->
        <div id="dashboard-section" class="card hidden">
            <h2>Kontrol Paneli</h2>
            
            <div class="stat-row">
                <span class="label">Giriş Yapan:</span>
                <span class="value" id="dashboard-user">...</span>
            </div>
            <div class="stat-row">
                <span class="label">Cihaz ID:</span>
                <span class="value" id="disp-id">...</span>
            </div>
            <div class="stat-row">
                <span class="label">Yazılım Versiyonu:</span>
                <span class="value" id="disp-ver">...</span>
            </div>
            <div class="stat-row">
                <span class="label">Tarih:</span>
                <span class="value" id="disp-date">...</span>
            </div>
            <div class="stat-row">
                <span class="label">Saat:</span>
                <span class="value" id="disp-time">...</span>
            </div>

            <button onclick="findDevice()" class="btn-success">🔊 Cihazı Bul</button>
            <button onclick="openDoor()" class="btn-door">🚪 Kapıyı Aç</button>

            <h3>Ayarlar</h3>
            <input type="text" id="new-id" placeholder="Yeni Cihaz ID">
            <button onclick="setDeviceId()" class="btn-secondary">ID Güncelle</button>
            <button onclick="syncTime()" class="btn-secondary">Saati Senkronize Et</button>
            
            <h3>WiFi Ayarları</h3>
            <h4>SSID (Ağ Adı)</h4>
            <input type="text" id="new-wifi-ssid" placeholder="Örn: MySchool_Gate">
            <h4>Yeni Şifre</h4>
            <input type="password" id="new-wifi-pass" placeholder="Min 8 Karakter">
            <button onclick="updateWifi()" class="btn-danger">WiFi Ayarlarını Değiştir</button>
            
            <h3>Giriş Ayarları</h3>
            <h4>Yeni Kullanıcı Adı</h4>
            <input type="text" id="new-web-user" placeholder="Kullanıcı Adı">
            <h4>Yeni Şifre</h4>
            <input type="password" id="new-web-pass" placeholder="Şifre">
            <button onclick="updateWebCreds()" class="btn-danger">Giriş Bilgilerini Değiştir</button>

            <hr style="margin-top:30px; border-color:#555">
            <button onclick="factoryReset()" class="btn-danger" style="margin-top:20px">⚠️ Fabrika Ayarlarına Dön</button>
        </div>
    </div>

    <div id="toast">Message</div>

    <script>
        async function login() {
            const user = document.getElementById('login-username').value;
            const pass = document.getElementById('login-password').value;
            
            if(!user || !pass) return showToast("Lütfen tüm alanları doldurun.");

            try {
                const res = await fetch('/login', {
                    method: 'POST',
                    headers: {'Content-Type': 'application/x-www-form-urlencoded'},
                    body: 'username=' + encodeURIComponent(user) + '&password=' + encodeURIComponent(pass)
                });
                if (res.ok) {
                    document.getElementById('login-section').classList.add('hidden');
                    document.getElementById('dashboard-section').classList.remove('hidden');
                    document.getElementById('dashboard-user').innerText = user; // Set locally for display
                    loadStatus();
                } else {
                    showToast("Hatalı Kullanıcı Adı veya Şifre!");
                }
            } catch (e) { showToast("Bağlantı Hatası"); }
        }

        async function loadStatus() {
            try {
                const res = await fetch('/status');
                const data = await res.json();
                document.getElementById('disp-id').innerText = data.device_id;
                document.getElementById('disp-ver').innerText = data.firmware_version;
                
                document.getElementById('disp-ver').innerText = data.firmware_version;
                
                // Format Date and Time: YYYY-MM-DD HH:MM:SS -> DD.MM.YYYY & HH:MM:SS
                if(data.current_time && data.current_time.length > 10) {
                    const parts = data.current_time.split(' '); // [YYYY-MM-DD, HH:MM:SS]
                    if(parts.length === 2) {
                        // Reformat Date: YYYY-MM-DD -> DD.MM.YYYY
                        const dParts = parts[0].split('-');
                        if(dParts.length === 3) {
                            const formattedDate = dParts[2] + '.' + dParts[1] + '.' + dParts[0];
                            document.getElementById('disp-date').innerText = formattedDate;
                        } else {
                            document.getElementById('disp-date').innerText = parts[0];
                        }
                        document.getElementById('disp-time').innerText = parts[1];
                    }
                } else {
                    document.getElementById('disp-date').innerText = data.current_time;
                }
                // Pre-fill inputs
                document.getElementById('new-wifi-ssid').value = data.wifi_ssid;
            } catch (e) { console.error(e); }
        }

        async function findDevice() {
             try {
                showToast("Sinyal Gönderiliyor...");
                await fetch('/find_device', { method: 'POST' });
            } catch (e) { showToast("Hata"); }
        }

        async function openDoor() {
             try {
                showToast("Kapı Açılıyor...");
                await fetch('/open_door', { method: 'POST' });
            } catch (e) { showToast("Hata"); }
        }

        async function setDeviceId() {
            const newId = document.getElementById('new-id').value;
            if(!newId) return showToast("ID boş olamaz");
            try {
                const res = await fetch('/set_id', {
                    method: 'POST',
                    headers: {'Content-Type': 'application/json'},
                    body: JSON.stringify({device_id: newId})
                });
                if (res.ok) { showToast("ID Güncellendi!"); loadStatus(); } 
                else showToast("Hata oluştu");
            } catch (e) { showToast("Bağlantı Hatası"); }
        }
        
        async function updateWifi() {
            const ssid = document.getElementById('new-wifi-ssid').value;
            const pass = document.getElementById('new-wifi-pass').value;
            
            if(!ssid || ssid.length < 3) return showToast("Geçersiz SSID");
            if(!pass || pass.length < 8) return showToast("Şifre en az 8 karakter olmalı");
             
            if(!confirm("WiFi Ayarlarını değiştirmek istediğinize emin misiniz? Cihaz yeniden başlayacaktır.")) return;

            try {
                const res = await fetch('/set_wifi_config', {
                    method: 'POST',
                    headers: {'Content-Type': 'application/json'},
                    body: JSON.stringify({wifi_ssid: ssid, wifi_pass: pass})
                });
                if (res.ok) {
                    alert("WiFi Ayarları Değiştirildi! Cihaz yeniden başlatılıyor.");
                    location.reload();
                } else showToast("Hata oluştu");
            } catch (e) { showToast("Bağlantı Hatası"); }
        }

        async function updateWebCreds() {
            const user = document.getElementById('new-web-user').value;
            const pass = document.getElementById('new-web-pass').value;
            
            if(!user || !pass) return showToast("Alanları doldurun");
            
            if(!confirm("Giriş bilgilerini değiştirmek istediğinize emin misiniz?")) return;

            try {
                const res = await fetch('/set_web_config', {
                    method: 'POST',
                    headers: {'Content-Type': 'application/json'},
                    body: JSON.stringify({web_user: user, web_pass: pass})
                });
                if (res.ok) {
                    alert("Bilgiler güncellendi. Lütfen tekrar giriş yapın.");
                    location.reload();
                } else showToast("Hata oluştu");
            } catch (e) { showToast("Bağlantı Hatası"); }
        }

        async function factoryReset() {
            const currentPass = prompt("Fabrika ayarlarına dönmek için lütfen MEVCUT ADMİN ŞİFRESİNİ girin:");
            if(!currentPass) return;

            try {
                const res = await fetch('/factory_reset', {
                    method: 'POST',
                    headers: {'Content-Type': 'application/x-www-form-urlencoded'},
                    body: 'password=' + encodeURIComponent(currentPass)
                });
                if (res.ok) {
                    alert("Cihaz sıfırlandı ve yeniden başlatılıyor...");
                    location.reload();
                } else {
                    alert("Hatalı Şifre! İşlem iptal edildi.");
                }
            } catch (e) { showToast("Bağlantı Hatası"); }
        }

        async function syncTime() {
            const now = new Date();
            const payload = {
                year: now.getFullYear(),
                month: now.getMonth() + 1,
                day: now.getDate(),
                hour: now.getHours(),
                minute: now.getMinutes(),
                second: now.getSeconds()
            };
            try {
                const res = await fetch('/set_time', {
                    method: 'POST',
                    headers: {'Content-Type': 'application/json'},
                    body: JSON.stringify(payload)
                });
                if (res.ok) { showToast("Saat Eşitlendi!"); loadStatus(); }
                else showToast("Hata oluştu");
            } catch (e) { showToast("Bağlantı Hatası"); }
        }

        function showToast(msg) {
            const t = document.getElementById('toast');
            t.innerText = msg;
            t.style.display = 'block';
            setTimeout(() => t.style.display = 'none', 3000);
        }
    </script>
</body>
</html>
)rawliteral";
