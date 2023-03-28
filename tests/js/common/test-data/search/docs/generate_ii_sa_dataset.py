import random
import json
import os
import sys
import numpy as np
import subprocess

try:
    from faker import Faker
except ModuleNotFoundError:
    subprocess.check_call([sys.executable, "-m", "pip", "install", 'Faker'])
finally:
    from faker import Faker

def generatePlaces():
    res = {}

    location = generatePlaces.fake.location_on_land()
    res["Koordinaten"] = [location[0], location[1]]
    res["Titel"] = location[2]

    possibleMissedItems = ["Bezeichnung", "istGut", "Merkmale"]
    numOfMissed = random.randrange(0, len(possibleMissedItems))
    
    missed = random.choices(possibleMissedItems, k=numOfMissed)
    for m in missed:
        if m == "Bezeichnung":
            res["Bezeichnung"] = generatePlaces.fake.text(50)
        elif m == "istGut":
            res["istGut"] = (random.randrange(0,100) % 2) == 1
        elif m == "Merkmale":
            res["Merkmale"] = []
            for _ in range(10):
                res["Merkmale"].append(generatePlaces.fake.word())

    return res

generatePlaces.fake = Faker()

def generatePersonalData():
    res = {}
    res["Name"] = generatePersonalData.fake.first_name()
    res["Familienname"] = generatePersonalData.fake.last_name()
    res["Geburtsdatum"] = generatePersonalData.fake.date()
    res["Setzt"] = []

    numOfSetzt = random.randint(0, 8)
    for _ in range(numOfSetzt):
        res["Setzt"].append(generatePlaces())

    return res
  
generatePersonalData.fake = Faker()

def generateHaustiere():
    res = {}
    res["Tiere"] = random.choice(generateHaustiere.tieren)
    res["name"] = generateHaustiere.fake.first_name()
    res["Alt"] = random.randint(0, 25)
    res["_key"] = random.randint(-10000, 10000)
    return res
  
generateHaustiere.fake = Faker()
generateHaustiere.tieren = [
        "HUND",
        "KATZE",
        "SCHWEIN",
        "KUH",
        "PFERD",
        "HENNE",
        "HASE",
        "VOGEL"
    ]

def main():

    filename = "ii_sa_dataset.json"
    if os.path.exists(filename):
        os.remove(filename)

    Familienmitglieder = [
        "Vater",
        "Mutter",
        "Schwester",
        "Bruder",
        "Sohn",
        "Tochter",
        "Ehefrau",
        "Ehemann",
        "Halbbruder",
        "Halbschwester",
        "Großmutter",
        "Großvater",
        "Enkelkinder",
        "Enkelin",
        "Enkel",
        "Schwägerin"
    ]

    resultCollection = {}
    ids = []

    docs = []
    collectionName = "docCollection"
    numOfDocs = 100
    for i in range(numOfDocs):
        key = str(collectionName) + "-" + str(i)
        ids.append(collectionName + "/" + key)
        doc = {
            "_key": key,
            "Familie": {},
            "Setzt": [],
            "Personal": {
                "ids": {
                    "_system": {
                        "magische Zahl": []
                    }
                }
            },
            "Haustiere": []
        }
        
        members = random.choices(Familienmitglieder, k=random.randrange(0, len(Familienmitglieder)))
        for m in members:
            doc["Familie"][m] = generatePersonalData()
        numOfSetzt = random.randint(0, 6)
        for _ in range(numOfSetzt):
           doc["Setzt"].append(generatePlaces())
        
        doc["Personal"]["ids"]["_system"]["magische Zahl"] = np.random.randint(-100,100, random.randint(0, 7)).tolist()
        for _ in range(0, random.randint(0, 6)):
            doc["Haustiere"].append(generateHaustiere())
        docs.append(doc)

    resultCollection[collectionName] = docs

    edges = []
    collectionName = "edgeCollection"
    numOfEdges = 300    
    for i in range(numOfEdges):
        _from, _to =  random.choices(ids, k=2)
        edges.append(
            {
                "_key": str(i),
                "_from": _from,
                "_to": _to,
                "Personal": generatePersonalData()
            }
        )
    resultCollection[collectionName] = edges

    with open(filename, 'w') as f:
        json.dump(resultCollection, f)        

if __name__ == "__main__":
    main()
