#!/usr/bin/env python3
import os
import glob
import time
import json
import socket
from flask import Flask, send_file, render_template_string, request, jsonify
import subprocess

app = Flask(__name__)

# Enhanced HTML with touch and scroll support
HTML_PAGE = """
<!DOCTYPE html>
<html>
<head>
  <title>Comma UI Live View</title>
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <style>
    body { 
      margin: 0; 
      background: black; 
      display: flex; 
      justify-content: center; 
      align-items: center; 
      height: 100vh;
      overflow: hidden;
    }
    #ui-container {
      position: relative;
      max-width: 100%;
      max-height: 100%;
    }
    img { 
      width: 100%; 
      height: auto; 
      touch-action: none;
      -webkit-user-select: none;
      -moz-user-select: none;
      -ms-user-select: none;
      user-select: none;
    }
    .touch-indicator {
      position: absolute;
      width: 40px;
      height: 40px;
      border: 2px solid #ff0000;
      border-radius: 50%;
      background: rgba(255, 0, 0, 0.2);
      pointer-events: none;
      transform: translate(-50%, -50%);
      animation: fadeOut 0.5s ease-out forwards;
    }
    .drag-indicator {
      position: absolute;
      width: 4px;
      background: #00ff00;
      pointer-events: none;
      opacity: 0.7;
    }
    @keyframes fadeOut {
      0% { opacity: 1; transform: translate(-50%, -50%) scale(1); }
      100% { opacity: 0; transform: translate(-50%, -50%) scale(1.5); }
    }
    .status {
      position: absolute;
      top: 10px;
      left: 10px;
      color: white;
      background: rgba(0,0,0,0.7);
      padding: 5px 10px;
      border-radius: 5px;
      font-family: Arial, sans-serif;
      font-size: 14px;
    }
    .instructions {
      position: absolute;
      bottom: 10px;
      left: 10px;
      color: white;
      background: rgba(0,0,0,0.7);
      padding: 5px 10px;
      border-radius: 5px;
      font-family: Arial, sans-serif;
      font-size: 12px;
      max-width: 300px;
    }
  </style>
</head>
<body>
  <div id="ui-container">
    <img src="/screenshot?_t={{ timestamp }}" id="ui" />
    <div class="status" id="status">Ready</div>
    <div class="instructions">
      <div>• Tap: Single click/touch</div>
      <div>• Scroll: Mouse wheel or two-finger touch</div>
      <div>• Drag: Hold and move (for sliders)</div>
    </div>
  </div>
  
  <script>
    let img = document.getElementById("ui");
    let container = document.getElementById("ui-container");
    let status = document.getElementById("status");
    
    let isDragging = false;
    let dragStart = {x: 0, y: 0};
    let lastTouchTime = 0;
    
    // Refresh image
    setInterval(() => {
      img.src = "/screenshot?_t=" + Date.now();
    }, 1000);
    
    function showTouchIndicator(x, y) {
      let indicator = document.createElement('div');
      indicator.className = 'touch-indicator';
      indicator.style.left = x + 'px';
      indicator.style.top = y + 'px';
      container.appendChild(indicator);
      
      setTimeout(() => {
        container.removeChild(indicator);
      }, 500);
    }
    
    function getDeviceCoordinates(clientX, clientY) {
      let rect = img.getBoundingClientRect();
      let relX = (clientX - rect.left) / rect.width;
      let relY = (clientY - rect.top) / rect.height;
      return {
        x: Math.round(relX * 2160),
        y: Math.round(relY * 1080),
        rectX: clientX - rect.left,
        rectY: clientY - rect.top
      };
    }
    
    function sendEvent(eventData) {
      fetch('/input', {
        method: 'POST',
        headers: {
          'Content-Type': 'application/json',
        },
        body: JSON.stringify(eventData)
      }).catch(err => {
        console.error('Event send failed:', err);
        status.textContent = 'Send failed';
      });
    }
    
    // Mouse wheel scrolling (desktop)
    img.addEventListener('wheel', function(e) {
      e.preventDefault();
      let coords = getDeviceCoordinates(e.clientX, e.clientY);
      
      let deltaY = e.deltaY;
      let scrollDirection = deltaY > 0 ? 'down' : 'up';
      let scrollAmount = Math.abs(deltaY);
      
      status.textContent = `Scroll ${scrollDirection} (${scrollAmount})`;
      
      sendEvent({
        type: 'scroll',
        x: coords.x,
        y: coords.y,
        deltaY: deltaY,
        direction: scrollDirection,
        amount: scrollAmount
      });
    });
    
    // Touch events for mobile
    let touchStartTime = 0;
    let initialTouch = null;
    
    img.addEventListener('touchstart', function(e) {
      e.preventDefault();
      touchStartTime = Date.now();
      
      if (e.touches.length === 1) {
        // Single touch - potential tap or drag
        initialTouch = e.touches[0];
        let coords = getDeviceCoordinates(initialTouch.clientX, initialTouch.clientY);
        
        dragStart = {x: coords.x, y: coords.y};
        isDragging = false;
        
        sendEvent({
          type: 'touchstart',
          x: coords.x,
          y: coords.y
        });
      } else if (e.touches.length === 2) {
        // Two finger touch - prepare for scroll
        status.textContent = 'Two finger scroll mode';
      }
    });
    
    img.addEventListener('touchmove', function(e) {
      e.preventDefault();
      
      if (e.touches.length === 1 && initialTouch) {
        // Single finger drag
        let touch = e.touches[0];
        let coords = getDeviceCoordinates(touch.clientX, touch.clientY);
        
        let deltaX = coords.x - dragStart.x;
        let deltaY = coords.y - dragStart.y;
        let distance = Math.sqrt(deltaX * deltaX + deltaY * deltaY);
        
        if (distance > 10 && !isDragging) {
          // Start dragging
          isDragging = true;
          status.textContent = 'Dragging...';
        }
        
        if (isDragging) {
          sendEvent({
            type: 'drag',
            x: coords.x,
            y: coords.y,
            startX: dragStart.x,
            startY: dragStart.y,
            deltaX: deltaX,
            deltaY: deltaY
          });
        }
      } else if (e.touches.length === 2) {
        // Two finger scroll
        let touch1 = e.touches[0];
        let touch2 = e.touches[1];
        
        // Calculate center point
        let centerX = (touch1.clientX + touch2.clientX) / 2;
        let centerY = (touch1.clientY + touch2.clientY) / 2;
        let coords = getDeviceCoordinates(centerX, centerY);
        
        // Calculate vertical movement for scrolling
        let currentY = (touch1.clientY + touch2.clientY) / 2;
        if (this.lastTwoFingerY !== undefined) {
          let deltaY = currentY - this.lastTwoFingerY;
          let scrollDirection = deltaY > 0 ? 'down' : 'up';
          
          status.textContent = `Two-finger scroll ${scrollDirection}`;
          
          sendEvent({
            type: 'scroll',
            x: coords.x,
            y: coords.y,
            deltaY: deltaY * 3, // Amplify for better sensitivity
            direction: scrollDirection,
            amount: Math.abs(deltaY * 3)
          });
        }
        this.lastTwoFingerY = currentY;
      }
    });
    
    img.addEventListener('touchend', function(e) {
      e.preventDefault();
      let touchDuration = Date.now() - touchStartTime;
      
      if (e.touches.length === 0) {
        if (initialTouch && !isDragging && touchDuration < 300) {
          // Quick tap
          let coords = getDeviceCoordinates(initialTouch.clientX, initialTouch.clientY);
          status.textContent = `Tap: ${coords.x}, ${coords.y}`;
          showTouchIndicator(coords.rectX, coords.rectY);
          
          sendEvent({
            type: 'tap',
            x: coords.x,
            y: coords.y
          });
        } else if (isDragging) {
          // End drag
          sendEvent({
            type: 'dragend',
            x: dragStart.x,
            y: dragStart.y
          });
        }
        
        isDragging = false;
        initialTouch = null;
        this.lastTwoFingerY = undefined;
        status.textContent = 'Ready';
      }
    });
    
    // Mouse events for desktop
    img.addEventListener('mousedown', function(e) {
      e.preventDefault();
      if (e.button === 0) { // Left button
        let coords = getDeviceCoordinates(e.clientX, e.clientY);
        dragStart = {x: coords.x, y: coords.y};
        isDragging = false;
        
        sendEvent({
          type: 'mousedown',
          x: coords.x,
          y: coords.y
        });
      }
    });
    
    img.addEventListener('mousemove', function(e) {
      if (e.buttons === 1) { // Left button held
        let coords = getDeviceCoordinates(e.clientX, e.clientY);
        
        let deltaX = coords.x - dragStart.x;
        let deltaY = coords.y - dragStart.y;
        let distance = Math.sqrt(deltaX * deltaX + deltaY * deltaY);
        
        if (distance > 5 && !isDragging) {
          isDragging = true;
          status.textContent = 'Dragging...';
        }
        
        if (isDragging) {
          sendEvent({
            type: 'drag',
            x: coords.x,
            y: coords.y,
            startX: dragStart.x,
            startY: dragStart.y,
            deltaX: deltaX,
            deltaY: deltaY
          });
        }
      }
    });
    
    img.addEventListener('mouseup', function(e) {
      e.preventDefault();
      if (e.button === 0) { // Left button
        let coords = getDeviceCoordinates(e.clientX, e.clientY);
        
        if (!isDragging) {
          // Simple click
          status.textContent = `Click: ${coords.x}, ${coords.y}`;
          showTouchIndicator(coords.rectX, coords.rectY);
          
          sendEvent({
            type: 'click',
            x: coords.x,
            y: coords.y
          });
        } else {
          // End drag
          sendEvent({
            type: 'dragend',
            x: coords.x,
            y: coords.y
          });
        }
        
        isDragging = false;
        status.textContent = 'Ready';
      }
    });
    
    // Prevent context menu
    img.addEventListener('contextmenu', function(e) {
      e.preventDefault();
    });
  </script>
</body>
</html>
"""

def wait_for_wifi(interface="wlan0", timeout=30, delay=2):
    print("Waiting for Wi-Fi connection...")
    for _ in range(timeout // delay):
        try:
            result = subprocess.run(["ip", "route", "show", "dev", interface],
                                  stdout=subprocess.PIPE, stderr=subprocess.DEVNULL, text=True)
            if result.stdout.strip():
                print("Wi-Fi is up.")
                return True
        except Exception:
            pass
        time.sleep(delay)
    print("Wi-Fi not detected after timeout.")
    return False

def clean_old_screenshots():
    print("Cleaning old screenshots...")
    files = glob.glob("/tmp/ui_frame_*.png")
    for f in files:
        try:
            os.remove(f)
        except Exception as e:
            print(f"Failed to delete {f}: {e}")
    print(f"Deleted {len(files)} screenshots.")

def send_input_to_ui(event_data):
    """Send input events to Qt application via Unix socket"""
    try:
        with socket.socket(socket.AF_UNIX, socket.SOCK_STREAM) as sock:
            sock.connect('/tmp/ui_touch_socket')
            message = json.dumps({
                **event_data,
                'timestamp': time.time()
            })
            sock.send(message.encode())
            return True
    except Exception as e:
        print(f"Failed to send input event: {e}")
        return False

@app.route('/')
def index():
    return render_template_string(HTML_PAGE, timestamp='init')

@app.route('/screenshot')
def latest_screenshot():
    files = sorted(glob.glob("/tmp/ui_frame_*.png"))
    if not files:
        return "No screenshot found", 404
    return send_file(files[-1], mimetype='image/png')

@app.route('/input', methods=['POST'])
def handle_input():
    data = request.get_json()
    event_type = data.get('type', 'click')
    
    print(f"Received {event_type} event: {data}")
    
    if send_input_to_ui(data):
        return jsonify({'status': 'success'})
    else:
        return jsonify({'status': 'error', 'message': 'Failed to send input event'}), 500

# Backward compatibility with old touch endpoint
@app.route('/touch', methods=['POST'])
def handle_touch():
    data = request.get_json()
    data['type'] = 'click'  # Convert old touch events to click events
    return handle_input()

if __name__ == '__main__':
    wait_for_wifi()
    clean_old_screenshots()
    print("Starting Flask server with touch and scroll support...")
    app.run(host='0.0.0.0', port=8081, debug=False)