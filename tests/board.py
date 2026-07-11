"""Board space indices (0..39), matching the engine's Space enum order (standard
US Monopoly board, GO=0, counter-clockwise). Prices per the Monopoly Party FAQ."""

GO = 0
MEDITERRANEAN = 1          # Brown, $60
COMMUNITY_CHEST_1 = 2
BALTIC = 3                 # Brown, $60
INCOME_TAX = 4
READING_RR = 5             # $200
ORIENTAL = 6               # Light blue, $100
CHANCE_1 = 7
VERMONT = 8                # Light blue, $100
CONNECTICUT = 9            # Light blue, $120
JAIL = 10
ST_CHARLES = 11            # Magenta, $140
ELECTRIC_CO = 12           # Utility, $150
STATES = 13                # Magenta, $140
VIRGINIA = 14              # Magenta, $160
PENNSYLVANIA_RR = 15       # $200
ST_JAMES = 16              # Orange, $180
COMMUNITY_CHEST_2 = 17
TENNESSEE = 18             # Orange, $180
NEW_YORK = 19              # Orange, $200
FREE_PARKING = 20
KENTUCKY = 21              # Red, $220
CHANCE_2 = 22
INDIANA = 23               # Red, $220
ILLINOIS = 24              # Red, $240
BO_RR = 25                 # $200
ATLANTIC = 26              # Yellow, $260
VENTNOR = 27               # Yellow, $260
WATER_WORKS = 28           # Utility, $150
MARVIN_GARDENS = 29        # Yellow, $280
GO_TO_JAIL = 30
PACIFIC = 31               # Green, $300
NORTH_CAROLINA = 32        # Green, $300
COMMUNITY_CHEST_3 = 33
PENNSYLVANIA_AVE = 34      # Green, $320
SHORT_LINE_RR = 35         # $200
CHANCE_3 = 36
PARK_PLACE = 37            # Blue, $350
LUXURY_TAX = 38
BOARDWALK = 39             # Blue, $400

PRICE = {
    MEDITERRANEAN: 60, BALTIC: 60, ORIENTAL: 100, VERMONT: 100, CONNECTICUT: 120,
    ST_CHARLES: 140, STATES: 140, VIRGINIA: 160, ST_JAMES: 180, TENNESSEE: 180,
    NEW_YORK: 200, KENTUCKY: 220, INDIANA: 220, ILLINOIS: 240, ATLANTIC: 260,
    VENTNOR: 260, MARVIN_GARDENS: 280, PACIFIC: 300, NORTH_CAROLINA: 300,
    PENNSYLVANIA_AVE: 320, PARK_PLACE: 350, BOARDWALK: 400,
    READING_RR: 200, PENNSYLVANIA_RR: 200, BO_RR: 200, SHORT_LINE_RR: 200,
    ELECTRIC_CO: 150, WATER_WORKS: 150,
}
