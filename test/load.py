import json
# id open respond question
with open('data/question.json') as json_data:
    #f = open("questionniare.csv","w")
    #w = csv.writer(f)
    d = json.load(json_data)
    #print (d)
    for key in d :      #key is id
        print(key)
        for key1 in d[key]:
            #print (key1)
            row=[]
            for key2 in d[key][key1]:
                if(key2!="data" and key2!="page"):
                    row.append(d[key][key1][key2])
                if(key2=="data"):
                    for key3 in d[key][key1][key2]:
                        if(key3=="OpenTime" or key3=="question0"):
                            print(key3)
                        else:
                            #nofrneoifreio
#for test