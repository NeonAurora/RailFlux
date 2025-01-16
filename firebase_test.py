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
                    "coordinates": [(60, 0), (60, 10)],
                    "occupied": False
                },
                "S2": {
                    "coordinates": [(60, 10), (60, 30)],
                    "occupied": False
                },
                "S3": {
                    "coordinates": [(60, 30), (60, 50)],
                    "occupied": False
                },
                "S4": {
                    "coordinates": [(60, 50), (60, 100)],
                    "occupied": False
                },
                "S5": {
                    "coordinates": [(60, 100), (60, 120)],
                    "occupied": False
                },
                "S6": {
                    "coordinates": [(60, 120), (60, 160)],
                    "occupied": False
                },
                "S7": {
                    "coordinates": [(60, 160), (60, 180)],
                    "occupied": False
                },
                "S8": {
                    "coordinates": [(60, 180), (60, 200)],
                    "occupied": False
                },
                "S9": {
                    "coordinates": [(60, 200), (60, 250)],
                    "occupied": False
                }
            }
        },

        "T2": {
            "segments": {
                "S1": {
                    "coordinates": [(70, 0), (70, 53)],
                    "occupied": False
                },
                "S2": {
                    "coordinates": [(70, 53), (70, 70)],
                    "occupied": False
                },
                "S3": {
                    "coordinates": [(70, 70), (70, 90)],
                    "occupied": False
                },
                "S4": {
                    "coordinates": [(70, 90), (70, 119)],
                    "occupied": False
                },
                "S5": {
                    "coordinates": [(70, 119), (70, 163)],
                    "occupied": False
                },
                "S6": {
                    "coordinates": [(70, 163), (70, 182)],
                    "occupied": False
                },
                "S7": {
                    "coordinates": [(70, 182), (70, 200)],
                    "occupied": False
                },
                "S8": {
                    "coordinates": [(70, 200), (70, 250)],
                    "occupied": False
                }
            }
        },

        "T3": {
            "segments": {
                "S1": {
                    "coordinates": [(70, 95), (65, 100)],
                    "occupied": False
                },
                "S2": {
                    "coordinates": [(65, 100), (60, 105)],
                    "occupied": False
                }
            }
        },

        "T4": {
            "segments": {
                "S1": {
                    "coordinates": [(40, 100), (40, 110)],
                    "occupied": False
                },
                "S2": {
                    "coordinates": [(40, 110), (40, 135)],
                    "occupied": False
                },
                "S3": {
                    "coordinates": [(40, 135), (40, 150)],
                    "occupied": False
                },
                "S4": {
                    "coordinates": [(40, 150), (40, 180)],
                    "occupied": False
                }
            }
        },

        "T5": {
            "segments": {
                "S1": {
                    "coordinates": [(60, 110), (40, 130)],
                    "occupied": False
                }
            }
        },

        "T6": {
            "segments": {
                "S1": {
                    "coordinates": [(40, 170), (60, 190)],
                    "occupied": False
                }
            }
        },

        "T7": {
            "segments": {
                "S1": {
                    "coordinates": [(90, 110), (90, 130)],
                    "occupied": False
                },
                "S2": {
                    "coordinates": [(90, 130), (90, 140)],
                    "occupied": False
                },
                "S3": {
                    "coordinates": [(90, 140), (90, 180)],
                    "occupied": False
                }
            }
        },

        "T8": {
            "segments": {
                "S1": {
                    "coordinates": [(70, 100), (90, 120)],
                    "occupied": False
                }
            }
        },

        "T9": {
            "segments": {
                "S1": {
                    "coordinates": [(90, 170), (70, 190)],
                    "occupied": False
                }
            }
        },
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
