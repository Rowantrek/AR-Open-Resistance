AudioSignalResClass {
 Inputs {
  IOPItemInputClass {
   id 1
   name "Offset"
   tl -2 -1
   children {
    2
   }
   valueMax 600000
  }
  IOPItemInputClass {
   id 3
   name "MusicTrackID"
   tl 2 105
   children {
    4
   }
   valueMax 100
  }
  IOPItemInputClass {
   id 7
   name "DJTrackID"
   tl -8 217
   children {
    8
   }
  }
  IOPItemInputClass {
   id 9
   name "BroadcastType"
   tl -26 337
   children {
    10
   }
  }
  IOPItemInputClass {
   id 11
   name "Volume"
   tl -26 449
   children {
    12
   }
  }
 }
 Outputs {
  IOPItemOutputClass {
   id 2
   name "Offset"
   tl 243 1
   input 1
  }
  IOPItemOutputClass {
   id 4
   name "MusicTrackID"
   tl 251 121
   input 3
  }
  IOPItemOutputClass {
   id 8
   name "DJTrackID"
   tl 252 217
   input 7
  }
  IOPItemOutputClass {
   id 10
   name "BroadcastType"
   tl 244 337
   input 9
  }
  IOPItemOutputClass {
   id 12
   name "Volume"
   tl 244 449
   input 11
  }
 }
 compiled IOPCompiledClass {
  visited {
   517 518 389 390 261 262 133 134 5 6
  }
  ins {
   IOPCompiledIn {
    data {
     1 2
    }
   }
   IOPCompiledIn {
    data {
     1 65538
    }
   }
   IOPCompiledIn {
    data {
     1 131074
    }
   }
   IOPCompiledIn {
    data {
     1 196610
    }
   }
   IOPCompiledIn {
    data {
     1 262146
    }
   }
  }
  outs {
   IOPCompiledOut {
    data {
     0
    }
   }
   IOPCompiledOut {
    data {
     0
    }
   }
   IOPCompiledOut {
    data {
     0
    }
   }
   IOPCompiledOut {
    data {
     0
    }
   }
   IOPCompiledOut {
    data {
     0
    }
   }
  }
  processed 10
  version 2
 }
}