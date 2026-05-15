import tkinter as tk
from tkinter import ttk, messagebox
import json
import base64
import webbrowser
import os
try:
    from Crypto.Cipher import AES
    from Crypto.Random import get_random_bytes
except ImportError:
    messagebox.showerror("Error", "PyCryptodome library not found. Please run: pip install pycryptodome")
    exit()

# Default Defaults
DEFAULT_KEY = "X/FJlKHJucOp4j1PhA2rP6qo7P8NkJEE"
DEFAULT_KEY = "X/FJlKHJucOp4j1PhA2rP6qo7P8NkJEE"
DEFAULT_ID = "00000"

def toggle_inputs():
    if var_master.get():
        entry_date.config(state='disabled')
        entry_start.config(state='disabled')
        entry_end.config(state='disabled')
    else:
        entry_date.config(state='normal')
        entry_start.config(state='normal')
        entry_end.config(state='normal')

def generate_qr():
    try:
        # 1. Get Inputs
        key_str = entry_key.get().strip()
        dev_id = entry_id.get().strip()
        date = entry_date.get().strip()
        start = entry_start.get().strip()
        end = entry_end.get().strip()
        
        if len(key_str) != 32:
            try:
                # Try decoding if user pasted base64 or hex? No, logic says it's a 32-char string in existing code.
                # The existing C++ code uses a char array of length 32 directly. 
                # "X/FJlKHJucOp4j1PhA2rP6qo7P8NkJEE" is exactly 32 chars.
                pass
            except:
                pass

        key_bytes = key_str.encode('utf-8')
        if len(key_bytes) != 32:
             messagebox.showerror("Error", f"Key must be exactly 32 bytes. Current: {len(key_bytes)}")
             return

        # 2. Prepare JSON
        if var_master.get():
             payload = {
                "checkinId": dev_id,
                "type": "MASTER"
            }
        else:
            payload = {
                "checkinId": dev_id,
                "reservationDate": date,
                "startTime": start,
                "endTime": end,
                "status": "CREATED"
            }
        
        json_str = json.dumps(payload)
        data = json_str.encode('utf-8')

        # 3. Encrypt AES-GCM
        iv = get_random_bytes(12)
        cipher = AES.new(key_bytes, AES.MODE_GCM, nonce=iv)
        ciphertext, tag = cipher.encrypt_and_digest(data)
        
        # Combined: IV + Cipher + Tag
        combined = iv + ciphertext + tag
        final_b64 = base64.b64encode(combined).decode('utf-8')
        
        # 4. Update UI
        text_output.delete(1.0, tk.END)
        text_output.insert(tk.END, final_b64)
        
        # 5. Generate and Open HTML
        create_html(final_b64, json_str)
        
    except Exception as e:
        messagebox.showerror("Error", str(e))

def create_html(b64_data, json_debug):
    html_content = f"""
<!DOCTYPE html>
<html>
<head>
    <title>Generated QR Code</title>
    <script src="https://cdnjs.cloudflare.com/ajax/libs/qrcodejs/1.0.0/qrcode.min.js"></script>
    <style>
        body {{ font-family: sans-serif; text-align: center; padding: 20px; }}
        #qrcode {{ margin: 20px auto; width: 300px; }}
        .box {{ background: #f0f0f0; padding: 10px; border-radius: 5px; margin: 10px auto; max-width: 800px; word-break: break-all; }}
    </style>
</head>
<body>
    <h1>Generated QR Code</h1>
    <div id="qrcode"></div>
    <h3>Encrypted Base64 Data:</h3>
    <div class="box">{b64_data}</div>
    <h3>Decrypted JSON Payload (Debug):</h3>
    <div class="box">{json_debug}</div>
    
    <script>
        new QRCode(document.getElementById("qrcode"), {{
            text: "{b64_data}",
            width: 300,
            height: 300,
            correctLevel: QRCode.CorrectLevel.M
        }});
    </script>
</body>
</html>
    """
    
    filename = "latest_generated_qr.html"
    with open(filename, "w", encoding="utf-8") as f:
        f.write(html_content)
    
    webbrowser.open('file://' + os.path.realpath(filename))

# --- GUI Setup ---
root = tk.Tk()
root.title("AES-GCM QR Generator Tool")
root.geometry("600x550")

frame = ttk.Frame(root, padding="20")
frame.pack(fill=tk.BOTH, expand=True)

# Grid Layout
row = 0

# MASTER CHECKBOX
var_master = tk.BooleanVar()
chk_master = ttk.Checkbutton(frame, text="Master QR (Device ID Only)", variable=var_master, command=toggle_inputs)
chk_master.grid(row=row, column=0, columnspan=2, sticky=tk.W, pady=5)
row += 1

# KEY
ttk.Label(frame, text="AES Key (32 chars):").grid(row=row, column=0, sticky=tk.W)
entry_key = ttk.Entry(frame, width=50)
entry_key.insert(0, DEFAULT_KEY)
entry_key.grid(row=row, column=1, sticky=tk.W, pady=5)
row += 1

# DEVICE ID
ttk.Label(frame, text="Device/Checkin ID:").grid(row=row, column=0, sticky=tk.W)
entry_id = ttk.Entry(frame, width=50)
entry_id.insert(0, DEFAULT_ID)
entry_id.grid(row=row, column=1, sticky=tk.W, pady=5)
row += 1

# DATE
ttk.Label(frame, text="Date (YYYY-MM-DD):").grid(row=row, column=0, sticky=tk.W)
entry_date = ttk.Entry(frame, width=50)
entry_date.insert(0, "2026-01-24")
entry_date.grid(row=row, column=1, sticky=tk.W, pady=5)
row += 1

# START TIME
ttk.Label(frame, text="Start Time (HH:MM):").grid(row=row, column=0, sticky=tk.W)
entry_start = ttk.Entry(frame, width=50)
entry_start.insert(0, "00:00")
entry_start.grid(row=row, column=1, sticky=tk.W, pady=5)
row += 1

# END TIME
ttk.Label(frame, text="End Time (HH:MM):").grid(row=row, column=0, sticky=tk.W)
entry_end = ttk.Entry(frame, width=50)
entry_end.insert(0, "23:59")
entry_end.grid(row=row, column=1, sticky=tk.W, pady=5)
row += 1

# BUTTON
btn = ttk.Button(frame, text="GENERATE QR & OPEN BROWSER", command=generate_qr)
btn.grid(row=row, column=0, columnspan=2, pady=20, sticky=tk.EW)
row += 1

# OUTPUT AREA
ttk.Label(frame, text="Output Base64:").grid(row=row, column=0, sticky=tk.W)
row += 1
text_output = tk.Text(frame, height=8, width=70)
text_output.grid(row=row, column=0, columnspan=2)

root.mainloop()
