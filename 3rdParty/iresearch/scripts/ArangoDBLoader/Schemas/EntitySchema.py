from mimesis.schema import Field, Schema
from mimesis import builtins

def getSchema():
  _ = Field('en', providers=[builtins.USASpecProvider])

  entityTypes = [
    "address","bankaccount","bankcard","case","charge","commonreference","caserecord",
    "emailaddress","flight","handset","interview","judgement","loyaltycard","meter","nationalidcard",
    "numberplate","operation","organisation","passport","person","phonenumber","pointofsale","prison",
    "punishment","simcard","suspect","vehicle","visa","pobox","pointofentry","ship","financialendpoint",
    "unknownparty" ]

  permissions = ['Public', 'Private', 'Department']
  statuses = ['Live', 'Disabled', 'Work In Progress']

  return Schema(schema=lambda: {
    '_key': _('identifier', mask='#####-#####-####'),
    'owner': _('full_name'),
    'permission': _('choice', items=permissions),
    'status': _('choice', items=statuses),
    'type': _('choice', items=entityTypes),
    'address': [ { "value": _('address') } ],
    'firstName': [ { "value": _('first_name') } ],
    'lastName': [ { "value": _('last_name') } ],
    'gender': [ { "value": _('gender') } ],
    'phone': [ { "value": _('telephone') } ],
    'nationality': [ { "value": _('nationality') } ],
    'occupation': [ { "value": _('occupation') } ],
    'sexual_orientation': [ { "value": _('sexual_orientation') } ],
    'age': [ { "value": _('age') } ],
    'bloodType': [ { "value": _('blood_type') } ],
    'dna': [ { "value": _('dna_sequence', length=128) } ],
    'height': [ { "value": _('height') } ],
    'emailaddress': [ { "value": _('person.email', domains=['test.com'], key=str.lower) } ],
    'bankaccount': [ { "value": _('tracking_number') } ],
    'passport': [ { "value": _('ssn') } ],
    'organisation': [ { "value": _('text.word') } ],
    'operation': [ { "value": _('text.word') } ],
    'interview': [ { "value": _('text') } ],
    'person': [ { "value": _('text.word') } ],
    'vehicle': [ { "value": _('text.word') } ],
    'suspect': [ { "value": _('text.swear_word') } ],
    'visa': [ { "value": _('uuid') } ],
    'flight': [ { "value": _('text.word') } ],
    'timestamp': [ {"value": _('timestamp', posix=False) } ],
  })

def main():
  print(getSchema().create())
  return

if __name__== "__main__":
  main()
