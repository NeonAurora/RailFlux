import firebase_admin
from firebase_admin import credentials, db

# Path to the Firebase service account key JSON file
cred = credentials.Certificate("firebase_admin_key.json")

# Initialize the Firebase Admin SDK with Realtime Database URL
firebase_admin.initialize_app(cred, {
    'databaseURL': 'https://railflux-default-rtdb.asia-southeast1.firebasedatabase.app/'
})

# Reference to the database
ref = db.reference('/')
print("Firebase has been successfully initialized.")
