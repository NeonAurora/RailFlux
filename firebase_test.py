import firebase_admin
from firebase_admin import credentials, db

# Initialize Firebase Admin
cred = credentials.Certificate("firebase_admin_key.json")
firebase_admin.initialize_app(cred, {
    'databaseURL': 'https://railflux-default-rtdb.asia-southeast1.firebasedatabase.app/'
})

# Reference to the root of the database
root_ref = db.reference('/')

# Write the new data to replace the old data
root_ref.set({
    "tracks": {
        "T1": {
            "segments": {
                "S1": {
                    "coordinates": [(60, 0), (60, 50)],
                    "occupied": False
                },
                "S2": {
                    "coordinates": [(60, 50), (60, 100)],
                    "occupied": False
                },
                "S3": {
                    "coordinates": [(60, 100), (60, 150)],
                    "occupied": False
                },
                "S4": {
                    "coordinates": [(60, 150), (60, 200)],
                    "occupied": False
                }
            }
        },
        "T2": {
            "segments": {
                "S1": {
                    "coordinates": [(80, 0), (80, 50)],
                    "occupied": False
                },
                "S2": {
                    "coordinates": [(80, 50), (80, 100)],
                    "occupied": False
                }
            }
        }
    },
    "trains": {
        "train1": {
            "speed": 20,
            "direction": "forward",
            "current_track": "T1",
            "current_segment": "S1"
        },
        "train2": {
            "speed": 25,
            "direction": "backward",
            "current_track": "T2",
            "current_segment": "S1"
        }
    }
})

print("New data written to Firebase.")

# Read back all the data from the database to verify it was written correctly
database_data = root_ref.get()
print("Data read from Firebase:", database_data)
