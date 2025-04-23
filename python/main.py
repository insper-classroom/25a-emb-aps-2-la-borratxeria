import sys
import glob
import serial
import time
import pyautogui
import keyboard
import tkinter as tk
from tkinter import ttk, messagebox

pyautogui.PAUSE = 0

def move_mouse(button, value):
    if button == 0:
        pyautogui.moveRel(value, 0)
    elif button == 1:
        pyautogui.moveRel(0, value)

def handle_button(button, value):
    # Mapear botões físicos para teclas
    key_map = {
        3: 'a',
        4: 'b',
        5: 'z',
        6: 'x'
    }
    if button not in key_map:
        return

    key = key_map[button]
    if value == 1:
        keyboard.press(key)
    else:
        keyboard.release(key)

def controle(ser):
    left_pressed = False
    right_pressed = False

    while True:
        packet = ser.read(4)
        # pacotes sempre têm 4 bytes e terminam em 0xFF
        if len(packet) != 4 or packet[3] != 0xFF:
            continue

        button = packet[0]
        value  = int.from_bytes(packet[1:3], byteorder='little', signed=True)

        # DEBUG
        if button in (8,9):
            print(f"[DEBUG] Tilt event → button={button}, value={value}")

        # Joystick X/Y
        if button in (0, 1):
            move_mouse(button, value)

        # Tilt esquerda (código 8)
        elif button == 8:
            if value == 1 and not left_pressed:
                keyboard.press('left')
                # garante que a outra seta esteja solta
                if right_pressed:
                    keyboard.release('right')
                    right_pressed = False
                left_pressed = True
            elif value == 0 and left_pressed:
                keyboard.release('left')
                left_pressed = False

        # Tilt direita (código 9)
        elif button == 9:
            if value == 1 and not right_pressed:
                keyboard.press('right')
                if left_pressed:
                    keyboard.release('left')
                    left_pressed = False
                right_pressed = True
            elif value == 0 and right_pressed:
                keyboard.release('right')
                right_pressed = False

        # Botões digitais
        else:
            handle_button(button, value)

def serial_ports():
    ports = []
    if sys.platform.startswith('win'):
        for i in range(1, 256):
            port = f'COM{i}'
            try:
                s = serial.Serial(port)
                s.close()
                ports.append(port)
            except (OSError, serial.SerialException):
                pass
    elif sys.platform.startswith(('linux', 'cygwin')):
        ports = glob.glob('/dev/tty[A-Za-z]*')
    elif sys.platform.startswith('darwin'):
        ports = glob.glob('/dev/tty.*')
    else:
        raise EnvironmentError('Plataforma não suportada para detecção de portas seriais.')
    return ports

def conectar_porta(port_name, root, botao_conectar, status_label, mudar_cor_circulo):
    if not port_name:
        messagebox.showwarning("Aviso", "Selecione uma porta serial antes de conectar.")
        return
    try:
        ser = serial.Serial(port_name, 115200, timeout=1)
        status_label.config(text=f"Conectado em {port_name}", foreground="green")
        mudar_cor_circulo("green")
        botao_conectar.config(text="Conectado")
        root.update()
        controle(ser)
    except KeyboardInterrupt:
        print("Encerrando via KeyboardInterrupt.")
    except Exception as e:
        messagebox.showerror("Erro de Conexão", f"Não foi possível conectar em {port_name}.\nErro: {e}")
        mudar_cor_circulo("red")
    finally:
        if 'ser' in locals() and ser.is_open:
            ser.close()
        status_label.config(text="Conexão encerrada.", foreground="red")
        mudar_cor_circulo("red")

def criar_janela():
    root = tk.Tk()
    root.title("Controle de Mouse")
    root.geometry("400x250")
    root.resizable(False, False)

    dark_bg = "#2e2e2e"
    dark_fg = "#ffffff"
    accent_color = "#007acc"
    root.configure(bg=dark_bg)

    style = ttk.Style(root)
    style.theme_use("clam")
    style.configure("TFrame", background=dark_bg)
    style.configure("TLabel", background=dark_bg, foreground=dark_fg, font=("Segoe UI", 11))
    style.configure("TButton", font=("Segoe UI", 10, "bold"), foreground=dark_fg, background="#444444", borderwidth=0)
    style.map("TButton", background=[("active", "#555555")])
    style.configure("Accent.TButton", font=("Segoe UI", 12, "bold"), foreground=dark_fg, background=accent_color, padding=6)
    style.map("Accent.TButton", background=[("active", "#005f9e")])
    style.configure("TCombobox", fieldbackground=dark_bg, background=dark_bg, foreground=dark_fg)

    frame_principal = ttk.Frame(root, padding="20")
    frame_principal.pack(expand=True, fill="both")

    titulo_label = ttk.Label(frame_principal, text="Controle de Mouse", font=("Segoe UI", 14, "bold"))
    titulo_label.pack(pady=(0, 10))

    porta_var = tk.StringVar(value="")
    botao_conectar = ttk.Button(
        frame_principal,
        text="Conectar e Iniciar Leitura",
        style="Accent.TButton",
        command=lambda: conectar_porta(porta_var.get(), root, botao_conectar, status_label, mudar_cor_circulo)
    )
    botao_conectar.pack(pady=10)

    footer_frame = tk.Frame(root, bg=dark_bg)
    footer_frame.pack(side="bottom", fill="x", padx=10, pady=(10, 0))

    status_label = tk.Label(footer_frame, text="Aguardando seleção de porta...", font=("Segoe UI", 11), bg=dark_bg, fg=dark_fg)
    status_label.grid(row=0, column=0, sticky="w")

    portas_disponiveis = serial_ports()
    if portas_disponiveis:
        porta_var.set(portas_disponiveis[0])
    port_dropdown = ttk.Combobox(footer_frame, textvariable=porta_var, values=portas_disponiveis, state="readonly", width=10)
    port_dropdown.grid(row=0, column=1, padx=10)

    circle_canvas = tk.Canvas(footer_frame, width=20, height=20, highlightthickness=0, bg=dark_bg)
    circle_item = circle_canvas.create_oval(2, 2, 18, 18, fill="red", outline="")
    circle_canvas.grid(row=0, column=2, sticky="e")

    footer_frame.columnconfigure(1, weight=1)

    def mudar_cor_circulo(cor):
        circle_canvas.itemconfig(circle_item, fill=cor)

    root.mainloop()

if __name__ == "__main__":
    criar_janela()
