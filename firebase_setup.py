# firebase_setup.py
import os
import sys
import firebase_admin
from firebase_admin import credentials, db

def resource_path(relative_path):
    """Get absolute path to resource, works for dev and PyInstaller"""
    try:
        # PyInstaller creates a temp folder and stores path in _MEIPASS
        base_path = sys._MEIPASS
    except Exception:
        base_path = os.path.abspath(".")

    return os.path.join(base_path, relative_path)

# Path to the Firebase service account key JSON file using resource_path
key_file = resource_path("firebase_admin_key.json")

# Initialize the Firebase Admin SDK with Realtime Database URL
if not firebase_admin._apps:  # Check if Firebase is already initialized
    try:
        cred = credentials.Certificate(key_file)
        firebase_admin.initialize_app(cred, {
            'databaseURL': 'https://railflux-default-rtdb.asia-southeast1.firebasedatabase.app/'
        })
        print(f"Firebase has been successfully initialized using key at: {key_file}")
    except Exception as e:
        print(f"Error initializing Firebase: {e}")
        print(f"Attempted to use key file at: {key_file}")
        print(f"Current working directory: {os.getcwd()}")
        # List files in current directory to help debugging
        print("Files in current directory:", os.listdir('.'))
        # This will allow the program to continue but log the error
        # You might want to handle this differently based on your needs