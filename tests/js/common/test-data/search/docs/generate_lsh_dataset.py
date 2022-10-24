import random
import json
import os
import sys
import uuid
import subprocess

def jaccard_similarity(list1, list2):
    intersection = len(list(set(list1).intersection(list2)))
    union = (len(set(list1)) + len(set(list2))) - intersection
    return float(intersection) / union

def makeStr(words, delimiter="#"):
    str = ""
    l = len(words)
    for i in range(l):
        str += words[i]
        if i != l - 1:
            str += delimiter
    return str

def main():

    try:
        from faker import Faker
    except ModuleNotFoundError:
        subprocess.check_call([sys.executable, "-m", "pip", "install", 'Faker'])
    finally:
        from faker import Faker

    filename = "lsh_dataset.json"
    if os.path.exists(filename):
        os.remove(filename)

    numOfCollections = 2
    seeds = [24, 42] # seeds for each collection

    numOfDocs = 40
    numOfWords = 25
    numOfDuplications = 10


    # This object will be written to json file
    resultCollections = {}

    # This collection will contain duplicates from all collections
    resultCollections["duplication_collection"] = [] 

    for collNum in range(numOfCollections):
        collName = "collection_{}".format(collNum)
        resultCollections[collName] = []
        fake = Faker()
        Faker.seed(seeds[collNum])
        random.seed(seeds[collNum])

        # Generate docs
        for _ in range(numOfDocs):
            words = []
            for i in range(numOfWords):
                words.append(fake.sentence(2))

            docJson = {
                "dataArr": words,
                "dataStr": makeStr(words, "#")
            }
            resultCollections[collName].append(docJson)

        # Generate duplicates
        dupIndexes = random.sample(range(numOfDocs), numOfDuplications)
        print(dupIndexes)
        for i in dupIndexes:
            uid = str(uuid.uuid4())
            doc = resultCollections[collName][i]
            resultCollections[collName][i]["dupId"] = uid
            dupWords = doc["dataArr"][0:random.randrange(numOfWords - 7, numOfWords)]
            # print(len(dupWords))
            for _ in range(numOfWords - len(dupWords)):
                dupWords.append(fake.sentence(5))

            dupDocJson = {
                "dataArr": dupWords,
                "dataStr": makeStr(dupWords, "#"),
                "jaccardScore": jaccard_similarity(dupWords, doc["dataArr"]),
                "dupId": uid
            }
            resultCollections["duplication_collection"].append(dupDocJson)
        
    random.shuffle(resultCollections["duplication_collection"])

    with open(filename, 'w') as f:
        json.dump(resultCollections, f)

if __name__ == "__main__":
    main()