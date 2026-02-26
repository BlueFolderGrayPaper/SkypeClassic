#include "models/Contact.h"

QString Contact::statusString() const {
    switch (status) {
        case ContactStatus::Online:       return "Online";
        case ContactStatus::Away:         return "Away";
        case ContactStatus::NotAvailable: return "Not Available";
        case ContactStatus::DoNotDisturb: return "Do Not Disturb";
        case ContactStatus::Invisible:    return "Invisible";
        case ContactStatus::Offline:      return "Offline";
    }
    return "Unknown";
}

QList<Contact> Contact::createMockContacts() {
    return {
        {1, "Echo / Sound Test Service", "echo123", "SKP-10001", ContactStatus::Online, ""},
        {2, "Alice Johnson", "alice.j", "SKP-20045", ContactStatus::Online, "Working from home today"},
        {3, "Bob Smith", "bobsmith42", "SKP-30012", ContactStatus::Away, "Back in 5 min"},
        {4, "Charlie Brown", "charlie_b", "SKP-40099", ContactStatus::Online, ""},
        {5, "Diana Prince", "diana.prince", "SKP-50033", ContactStatus::DoNotDisturb, "In a meeting"},
        {6, "Edward Norton", "ed_norton", "SKP-60071", ContactStatus::Offline, ""},
        {7, "Fiona Green", "fiona.g", "SKP-70028", ContactStatus::Away, "Lunch break"},
        {8, "George Miller", "gmiller", "SKP-80056", ContactStatus::Offline, "On vacation until Monday"},
        {9, "Helen Troy", "helen_t", "SKP-90014", ContactStatus::Online, "Call me anytime!"},
        {10, "Ivan Petrov", "ivanp", "SKP-10087", ContactStatus::Offline, ""},
    };
}
