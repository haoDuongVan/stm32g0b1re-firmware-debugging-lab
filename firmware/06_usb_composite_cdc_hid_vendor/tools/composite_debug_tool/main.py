"""
main.py - Entry point for STM32 USB Composite debug GUI.

Usage:
    cd tools/composite_debug_tool
    pip install -r requirements.txt
    python main.py

Requirements:
    - libusbK installed on Interface 3 (Vendor Data) via Zadig
    - HID and CDC keep their inbox drivers
"""

from gui import App


def main():
    app = App()
    app.mainloop()


if __name__ == "__main__":
    main()
