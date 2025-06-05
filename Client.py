import sys
import json
import socket
import threading
from PyQt5.QtWidgets import (QApplication, QMainWindow, QWidget, QVBoxLayout, QHBoxLayout, 
                             QTextEdit, QLineEdit, QPushButton, QListWidget, QLabel, 
                             QTabWidget, QMessageBox, QInputDialog)
from PyQt5.QtCore import Qt, pyqtSignal, QObject, QTimer

class SignalEmitter(QObject):
    message_received = pyqtSignal(dict)
    auth_result = pyqtSignal(dict)
    register_result = pyqtSignal(dict)
    chat_history = pyqtSignal(dict)
    team_created = pyqtSignal(dict)
    user_added = pyqtSignal(dict)
    chat_list_received = pyqtSignal(dict)

class ChatClient:
    def __init__(self, host, port):
        self.host = host
        self.port = port
        self.socket = None
        self.connected = False
        self.username = ""
        self.signal_emitter = SignalEmitter()
        
    def connect(self):
        if self.connected:
            return True
            
        try:
            self.socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            self.socket.connect((self.host, self.port))
            self.connected = True
            threading.Thread(target=self.receive_messages, daemon=True).start()
            return True
        except Exception as e:
            print(f"Connection error: {e}")
            return False
    
    def receive_messages(self):
        buffer = ""
        while self.connected:
            try:
                data = self.socket.recv(4096).decode('utf-8')
                if not data:
                    break
                    
                buffer += data
                while '\0' in buffer:
                    message, buffer = buffer.split('\0', 1)
                    try:
                        msg = json.loads(message)
                        self.handle_server_message(msg)
                    except json.JSONDecodeError:
                        print(f"Invalid JSON: {message}")
            except Exception as e:
                print(f"Receive error: {e}")
                self.disconnect()
                break
    
    def handle_server_message(self, msg):
        msg_type = msg.get("type")
        
        if msg_type in ["message", "team_message"]:
            self.signal_emitter.message_received.emit(msg)
        elif msg_type == "auth_response":
            self.signal_emitter.auth_result.emit(msg)
        elif msg_type == "register_response":
            self.signal_emitter.register_result.emit(msg)
        elif msg_type == "chat_messages":
            self.signal_emitter.chat_history.emit(msg)
        elif msg_type == "team_created":
            self.signal_emitter.team_created.emit(msg)
        elif msg_type == "user_added":
            self.signal_emitter.user_added.emit(msg)
        elif msg_type == "chat_list":
            self.signal_emitter.chat_list_received.emit(msg)
        elif msg_type == "error":
            print(f"Server error: {msg.get('message', 'Unknown error')}")
    
    def send_message(self, message):
        if not self.connected:
            return False
        
        try:
            self.socket.sendall((json.dumps(message) + '\0').encode('utf-8'))
            return True
        except Exception as e:
            print(f"Send error: {e}")
            self.disconnect()
            return False
    
    def authenticate(self, username, password_hash):
        msg = {
            "type": "auth",
            "username": username,
            "password_hash": password_hash
        }
        return self.send_message(msg)
    
    def register(self, username, password_hash):
        msg = {
            "type": "register",
            "username": username,
            "password_hash": password_hash
        }
        return self.send_message(msg)
    
    def send_chat_message(self, to, content, is_team=False):
        msg = {
            "type": "team_message" if is_team else "message",
            "to": to,
            "content": content
        }
        return self.send_message(msg)
    
    def get_chat_history(self, chat_id, is_team=False):
        msg = {
            "type": "get_chat_messages",
            "chat_id": chat_id,
            "is_team": is_team
        }
        return self.send_message(msg)
    
    def create_team(self, team_name):
        msg = {
            "type": "create_team",
            "team_name": team_name
        }
        return self.send_message(msg)
    
    def invite_to_team(self, team_name, username):
        msg = {
            "type": "invite_to_team",
            "team_name": team_name,
            "user": username
        }
        return self.send_message(msg)
    
    def get_chat_list(self):
        msg = {
            "type": "get_chat_list"
        }
        return self.send_message(msg)
    
    def disconnect(self):
        if self.connected:
            self.connected = False
            try:
                self.socket.close()
            except:
                pass
            finally:
                self.socket = None

class LoginWindow(QWidget):
    def __init__(self, client):
        super().__init__()
        self.client = client
        self.init_ui()
        
    def init_ui(self):
        self.setWindowTitle('Чат - Авторизация')
        self.setFixedSize(300, 200)
        
        layout = QVBoxLayout()
        
        self.username_input = QLineEdit()
        self.username_input.setPlaceholderText('Логин')
        layout.addWidget(self.username_input)
        
        self.password_input = QLineEdit()
        self.password_input.setPlaceholderText('Пароль')
        self.password_input.setEchoMode(QLineEdit.Password)
        layout.addWidget(self.password_input)
        
        buttons_layout = QHBoxLayout()
        
        self.login_button = QPushButton('Войти')
        self.login_button.clicked.connect(self.handle_login)
        buttons_layout.addWidget(self.login_button)
        
        self.register_button = QPushButton('Регистрация')
        self.register_button.clicked.connect(self.handle_register)
        buttons_layout.addWidget(self.register_button)
        
        layout.addLayout(buttons_layout)
        
        self.status_label = QLabel()
        self.status_label.setAlignment(Qt.AlignCenter)
        layout.addWidget(self.status_label)
        
        self.setLayout(layout)
        
        self.client.signal_emitter.auth_result.connect(self.handle_auth_result)
        self.client.signal_emitter.register_result.connect(self.handle_register_result)
    
    def handle_login(self):
        username = self.username_input.text()
        password = self.password_input.text()
        
        if not username or not password:
            self.status_label.setText('Введите логин и пароль')
            return
            
        self.client.disconnect()
        
        if not self.client.connect():
            self.status_label.setText('Ошибка подключения к серверу')
            return
            
        self.status_label.setText('Подключение...')
        self.client.authenticate(username, password)
    
    def handle_register(self):
        username = self.username_input.text()
        password = self.password_input.text()
        
        if not username or not password:
            self.status_label.setText('Введите логин и пароль')
            return
            
        self.client.disconnect()
        
        if not self.client.connect():
            self.status_label.setText('Ошибка подключения к серверу')
            return
            
        self.status_label.setText('Регистрация...')
        self.client.register(username, password)
    
    def handle_auth_result(self, response):
        if response.get("status") == "success":
            self.client.username = response.get("username", "")
            self.close()
            self.chat_window = ChatWindow(self.client)
            self.chat_window.show()
        else:
            self.status_label.setText(response.get("message", "Ошибка авторизации"))
            self.client.disconnect()
    
    def handle_register_result(self, response):
        message = response.get("message", "")
        self.client.disconnect()
        
        if message == "The user already exists":
            self.status_label.setText('Пользователь уже существует')
        elif message == "Registration error":
            self.status_label.setText('Ошибка регистрации')
        else:
            self.status_label.setText('Регистрация успешна. Теперь войдите.')
            self.username_input.clear()
            self.password_input.clear()

class ChatWindow(QMainWindow):
    def __init__(self, client):
        super().__init__()
        self.client = client
        self.current_chat = None
        self.current_chat_is_team = False
        self.init_ui()
        
        self.client.signal_emitter.message_received.connect(self.handle_message)
        self.client.signal_emitter.chat_history.connect(self.handle_chat_history)
        self.client.signal_emitter.team_created.connect(self.handle_team_created)
        self.client.signal_emitter.user_added.connect(self.handle_user_added)
        self.client.signal_emitter.chat_list_received.connect(self.handle_chat_list)
        
        self.update_timer = QTimer()
        self.update_timer.timeout.connect(self.load_chat_list)
        self.update_timer.start(30000)
        
        self.load_chat_list()
    
    def init_ui(self):
        self.setWindowTitle(f'Чат - {self.client.username}')
        self.setGeometry(100, 100, 800, 600)
        
        central_widget = QWidget()
        self.setCentralWidget(central_widget)
        
        main_layout = QHBoxLayout()
        central_widget.setLayout(main_layout)
        
        left_panel = QVBoxLayout()
        left_panel.setContentsMargins(0, 0, 0, 0)
        
        self.tabs = QTabWidget()
        
        self.users_tab = QWidget()
        users_layout = QVBoxLayout()
        self.users_list = QListWidget()
        self.users_list.itemClicked.connect(self.user_selected)
        users_layout.addWidget(QLabel('Пользователи:'))
        users_layout.addWidget(self.users_list)
        self.users_tab.setLayout(users_layout)
        
        self.teams_tab = QWidget()
        teams_layout = QVBoxLayout()
        self.teams_list = QListWidget()
        self.teams_list.itemClicked.connect(self.team_selected)
        
        self.create_team_button = QPushButton('Создать группу')
        self.create_team_button.clicked.connect(self.handle_create_team)
        
        self.invite_button = QPushButton('Пригласить в группу')
        self.invite_button.clicked.connect(self.invite_to_team)
        self.invite_button.setEnabled(False)
        
        teams_layout.addWidget(QLabel('Группы:'))
        teams_layout.addWidget(self.teams_list)
        teams_layout.addWidget(self.create_team_button)
        teams_layout.addWidget(self.invite_button)
        self.teams_tab.setLayout(teams_layout)
        
        self.tabs.addTab(self.users_tab, "Пользователи")
        self.tabs.addTab(self.teams_tab, "Группы")
        
        left_panel.addWidget(self.tabs)
        
        right_panel = QVBoxLayout()
        
        self.chat_label = QLabel()
        self.chat_label.setAlignment(Qt.AlignCenter)
        right_panel.addWidget(self.chat_label)
        
        self.chat_display = QTextEdit()
        self.chat_display.setReadOnly(True)
        right_panel.addWidget(self.chat_display)
        
        self.message_input = QLineEdit()
        self.message_input.setPlaceholderText('Введите сообщение...')
        self.message_input.returnPressed.connect(self.send_message)
        right_panel.addWidget(self.message_input)
        
        self.send_button = QPushButton('Отправить')
        self.send_button.clicked.connect(self.send_message)
        right_panel.addWidget(self.send_button)
        
        main_layout.addLayout(left_panel, 1)
        main_layout.addLayout(right_panel, 2)
    
    def load_chat_list(self):
        self.client.get_chat_list()
    
    def handle_chat_list(self, msg):
        data = msg.get("data", {})
        
        current_user = self.users_list.currentItem().text() if self.users_list.currentItem() else None
        current_team = self.teams_list.currentItem().text() if self.teams_list.currentItem() else None
        
        self.users_list.clear()
        self.teams_list.clear()
        
        users = data.get("users", [])
        self.users_list.addItems(users)
        
        teams = data.get("teams", [])
        for team in teams:
            self.teams_list.addItem(team["team_name"])
        
        if current_user and current_user in users:
            items = self.users_list.findItems(current_user, Qt.MatchExactly)
            if items:
                self.users_list.setCurrentItem(items[0])
        
        if current_team:
            items = self.teams_list.findItems(current_team, Qt.MatchExactly)
            if items:
                self.teams_list.setCurrentItem(items[0])
    
    def user_selected(self, item):
        self.current_chat = item.text()
        self.current_chat_is_team = False
        self.chat_label.setText(f'Чат с {self.current_chat}')
        self.chat_display.clear()
        self.client.get_chat_history(self.current_chat, False)
        self.invite_button.setEnabled(False)
    
    def team_selected(self, item):
        self.current_chat = item.text()
        self.current_chat_is_team = True
        self.chat_label.setText(f'Группа: {self.current_chat}')
        self.chat_display.clear()
        self.client.get_chat_history(self.current_chat, True)
        self.invite_button.setEnabled(True)
    
    def send_message(self):
        if not self.current_chat or not self.message_input.text():
            return
            
        message_text = self.message_input.text()
        is_team = self.current_chat_is_team
        
        self.client.send_chat_message(
            self.current_chat, 
            message_text,
            is_team
        )
        self.message_input.clear()
    
    def handle_message(self, msg):
        if msg["type"] == "message":
            if (msg["from"] == self.current_chat and not self.current_chat_is_team):
                self.display_message(msg["from"], msg["content"])
            elif (msg["to"] == self.current_chat and msg["from"] == self.client.username and not self.current_chat_is_team):
                self.display_message(msg["content"])
        elif msg["type"] == "team_message":
            if msg["to"] == self.current_chat and self.current_chat_is_team:
                self.display_message(f"{msg['from']}", msg["content"])
    
    def display_message(self, *args):
        if len(args) == 1:
            self.chat_display.append(f"{args[0]}")
        elif len(args) == 2:
            self.chat_display.append(f"<b>{args[0]}:</b> {args[1]}")
    
    def handle_chat_history(self, msg):
        if msg["chat_id"] == self.current_chat and msg["is_team"] == self.current_chat_is_team:
            for message in msg["messages"]:
                sender = message["from"]
                content = message["content"]
                if sender == self.client.username:
                    self.display_message(content)
                else:
                    if msg["is_team"]:
                        self.display_message(f"{sender}", content)
                    else:
                        self.display_message(sender, content)
    
    def handle_create_team(self):
        team_name, ok = QInputDialog.getText(
            self, 'Создание группы', 'Введите название группы:'
        )
        if ok and team_name:
            self.client.create_team(team_name)
    
    def handle_team_created(self, msg):
        team_name = msg.get("team_name")
        if team_name:
            QMessageBox.information(self, 'Группа создана', f'Группа "{team_name}" успешно создана!')
            self.load_chat_list()
    
    def invite_to_team(self):
        if not self.current_chat_is_team:
            return
            
        user, ok = QInputDialog.getText(
            self, 'Приглашение в группу', 
            f'Введите имя пользователя для приглашения в {self.current_chat}:'
        )
        if ok and user:
            self.client.invite_to_team(self.current_chat, user)
    
    def handle_user_added(self, msg):
        team_name = msg.get("team_name")
        if team_name:
            self.load_chat_list()
            QMessageBox.information(self, 'Пользователь добавлен', 
                                  f'Пользователь добавлен в группу "{team_name}"')
    
    def closeEvent(self, event):
        self.update_timer.stop()
        self.client.disconnect()
        event.accept()

if __name__ == '__main__':
    app = QApplication(sys.argv)
    
    HOST = '127.0.0.1'
    PORT = 52777
    
    client = ChatClient(HOST, PORT)
    login_window = LoginWindow(client)
    login_window.show()
    
    sys.exit(app.exec_())