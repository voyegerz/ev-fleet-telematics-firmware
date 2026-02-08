from flask import Flask, request, jsonify, render_template
from flask_sqlalchemy import SQLAlchemy
from datetime import datetime
import os

app = Flask(__name__)

# Database Config (SQLite)
# Ensure the instance folder exists or put db in current dir
db_path = os.path.join(os.path.abspath(os.path.dirname(__file__)), 'telematics.db')
app.config['SQLALCHEMY_DATABASE_URI'] = f'sqlite:///{db_path}'
app.config['SQLALCHEMY_TRACK_MODIFICATIONS'] = False

db = SQLAlchemy(app)

# ==========================================
# 🗄️ DATABASE MODELS
# ==========================================
class DeviceStatus(db.Model):
    id = db.Column(db.Integer, primary_key=True)
    # Telemetry
    lat = db.Column(db.Float, default=0.0)
    lng = db.Column(db.Float, default=0.0)
    speed = db.Column(db.Float, default=0.0)
    
    # Hardware State (Reported by ESP32)
    relay_state = db.Column(db.Boolean, default=False)
    led_state = db.Column(db.Boolean, default=False)
    gps_fix = db.Column(db.Boolean, default=False)
    
    # Last Update
    last_seen = db.Column(db.DateTime, default=datetime.utcnow)

    # Command Queue (What we WANT the device to do)
    target_relay_state = db.Column(db.Boolean, default=False)

    def to_dict(self):
        # Calculate online status (threshold: 20 seconds)
        diff = (datetime.utcnow() - self.last_seen).total_seconds()
        is_online = diff < 35
        
        return {
            "lat": self.lat,
            "lng": self.lng,
            "speed": self.speed,
            "relay_state": self.relay_state,
            "led_state": self.led_state,
            "gps_fix": self.gps_fix,
            "online": is_online,
            "seconds_ago": int(diff),
            "last_seen": self.last_seen.strftime("%Y-%m-%d %H:%M:%S"),
            "target_relay_state": self.target_relay_state
        }

# Initialize DB
with app.app_context():
    db.create_all()
    # Create default device if not exists
    if not DeviceStatus.query.first():
        new_device = DeviceStatus(lat=0.0, lng=0.0, speed=0.0)
        db.session.add(new_device)
        db.session.commit()

# ==========================================
# 🚀 API ENDPOINTS (ESP32)
# ==========================================
@app.route('/api/update', methods=['POST'])
def update_telemetry():
    """
    ESP32 sends JSON: { "lat": 12.34, "lng": 56.78, "speed": 10, "relay_status": 1, "led_status": 1 }
    Server responds:  { "relay_cmd": 1 } (Target state)
    """
    data = request.json
    if not data:
        return jsonify({"error": "No data"}), 400

    device = DeviceStatus.query.first()
    
    # 1. Update DB with real-world data from ESP32
    device.lat = data.get('lat', device.lat)
    device.lng = data.get('lng', device.lng)
    device.speed = data.get('speed', device.speed)
    
    # Status is 1 (HIGH) or 0 (LOW)
    device.relay_state = bool(data.get('relay_status', 0))
    device.led_state = bool(data.get('led_status', 0))
    device.gps_fix = bool(data.get('gps_fix', 0))
    device.last_seen = datetime.utcnow()
    
    db.session.commit()

    # 2. Respond with the COMMAND (Target State)
    # The ESP32 will read this and switch the relay if needed
    response_payload = {
        "relay_cmd": 1 if device.target_relay_state else 0
    }
    
    return jsonify(response_payload)

# ==========================================
# 🖥️ WEB DASHBOARD ENDPOINTS 
# ==========================================
@app.route('/')
def index():
    return render_template('index.html')

@app.route('/lock')
def lock_page():
    return render_template('lock.html')

@app.route('/unlock')
def unlock_page():
    return render_template('unlock.html')

@app.route('/scan')
def scan_page():
    return render_template('scan.html')

@app.route('/api/status', methods=['GET'])
def get_status():
    """Frontend polls this to update map and buttons"""
    device = DeviceStatus.query.first()
    return jsonify(device.to_dict())

@app.route('/api/control', methods=['POST'])
def control_device():
    """Frontend sends { "action": "ON" } or "OFF" """
    data = request.json
    action = data.get('action')
    
    device = DeviceStatus.query.first()
    
    if action == "ON":
        device.target_relay_state = True
    elif action == "OFF":
        device.target_relay_state = False
        
    db.session.commit()
    return jsonify({"status": "updated", "target": device.target_relay_state})

if __name__ == '__main__':
    # Use 0.0.0.0 for external access
    app.run(host='0.0.0.0', port=5000, debug=True)
