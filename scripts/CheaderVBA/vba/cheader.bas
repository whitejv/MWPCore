Attribute VB_Name = "Module1"
    Sub GenerateCHeader()

    Dim ws As Worksheet
    Dim outputFile As String
    Dim i As Long, J As Long, startRow As Long
    Dim fileContent As String
    Dim blockName As String
    Dim blockNamePrefix As String
    Dim structName As String
    
    Set ws = ThisWorkbook.ActiveSheet
    outputFile = ThisWorkbook.Path & "\IrrigationHeader.h"
    
     i = 1 ' Initialize row pointer
    fileContent = "" ' Initialize the file content
    
    Do While Not IsEmpty(ws.Cells(i, 1))
    
    
            ' Extract block name and format it
            blockName = Replace(ws.Cells(i + 1, 2).Value, " ", "") & ""
            blockNamePrefix = Replace(ws.Cells(i + 1, 2).Value, " ", "_") & "_"
            blockNameLen = Replace(ws.Cells(i + 8, 2).Value, " ", "_") & "_"
            structName = Replace(ws.Cells(i + 3, 2).Value, "", "")
            ' Header content
            fileContent = fileContent & "/*" & vbCr
            startRow = i
            For J = startRow To startRow + 8 ' Loop till 9 to avoid adding an extra line break for the 10th line
                fileContent = fileContent & "* " & ws.Cells(J, 1).Value & ": " & ws.Cells(J, 2).Value & vbCr
            Next J
            i = J
            
            ' Table content for the block
            Do While ws.Cells(i, 1).Value <> "" ' Check for blank cell in column 1 to determine the end of data
                fileContent = fileContent & "*  " & ws.Cells(i, 1).Value & "        " & ws.Cells(i, 2).Value & "            " & ws.Cells(i, 3).Value & "        " & ws.Cells(i, 4).Value & "        " & ws.Cells(i, 5).Value & "        " & ws.Cells(i, 6).Value & "        " & ws.Cells(i, 7).Value & "        " & ws.Cells(i, 8).Value & vbCr
                i = i + 1
            Loop
            fileContent = fileContent & "*/" & vbCrLf
        
             ' Definitions for the block
            fileContent = fileContent & "const char " & UCase(blockNamePrefix) & "CLIENTID[] =    """ & ws.Cells(startRow + 6, 2).Value & """ " & ";" & vbCr
            
            ' Check if the cell for the block name is not empty
            If Trim(ws.Cells(startRow + 7, 2).Value) <> "" Then
                ' If not empty, prepend "mwp/" to the value from the cell
                fileContent = fileContent & "const char " & UCase(blockNamePrefix) & "TOPICID[] =  ""mwp/" & ws.Cells(startRow + 7, 2).Value & """" & ";" & vbCr
                fileContent = fileContent & "const char " & UCase(blockNamePrefix) & "JSONID[] =  ""mwp/json/" & ws.Cells(startRow + 7, 2).Value & """" & ";" & vbCr
            Else
                ' If the cell is empty, build the topic ID using a different method
                fileContent = fileContent & "const char " & UCase(blockNamePrefix) & "TOPICID[] =  ""mwp/" & ws.Cells(startRow + 5, 2).Value & "/" & ws.Cells(startRow + 4, 2).Value & "/" & ws.Cells(startRow + 3, 2).Value & "/" & ws.Cells(startRow, 2).Value & """" & ";" & vbCr
                fileContent = fileContent & "const char " & UCase(blockNamePrefix) & "JSONID[] =  ""mwp/json/" & ws.Cells(startRow + 5, 2).Value & "/" & ws.Cells(startRow + 4, 2).Value & "/" & ws.Cells(startRow + 3, 2).Value & "/" & ws.Cells(startRow, 2).Value & """" & ";" & vbCr
            End If

            fileContent = fileContent & "#define " & UCase(blockNamePrefix) & "LEN " & ws.Cells(startRow + 8, 2).Value & vbCrLf
            fileContent = fileContent & "union   " & UCase(blockNamePrefix) & "  {" & vbCr
            fileContent = fileContent & "   " & ws.Cells(startRow + 10, 2).Value & "     data_payload[" & UCase(blockNamePrefix) & "LEN] ;" & vbCrLf
        
        ' Struct definitions for the block
        fileContent = fileContent & "   struct  {" & vbCr
        J = startRow + 10
        While ws.Cells(J, 3).Value <> "" And Not IsEmpty(ws.Cells(J, 3))
            fileContent = fileContent & "      " & ws.Cells(J, 2).Value & "   " & ws.Cells(J, 3).Value & vbTab & ";" & vbTab & vbTab & "//   " & ws.Cells(J, 9).Value & vbCr
            J = J + 1
        Wend
        fileContent = fileContent & "   }  " & LCase(structName) & "  ;" & vbCr
        fileContent = fileContent & "}  ;" & vbCr
        fileContent = fileContent & "union  " & UCase(blockNamePrefix) & "  " & blockName & "_  ;" & vbCrLf

        ' Variable names array for the block
        fileContent = fileContent & "char* " & LCase(blockNamePrefix) & "ClientData_var_name [] = { " & vbCr
        J = startRow + 10
        While ws.Cells(J, 3).Value <> "" And Not IsEmpty(ws.Cells(J, 3))
            If ws.Cells(startRow, 2).Value <> "" Then
                fileContent = fileContent & "    """ & ws.Cells(startRow, 2) & ":" & ws.Cells(J, 3).Value & """," & vbCr
            Else
                fileContent = fileContent & "    """ & ws.Cells(J, 3).Value & """," & vbCr
            End If
            J = J + 1
        Wend
        fileContent = fileContent & "}  ;" & vbCrLf

        ' Move pointer to next block or end
        i = J + 1

    Loop
    
    ' Write to file
    Dim fileNum As Integer
    fileNum = FreeFile
    Open outputFile For Output As fileNum
    Print #fileNum, fileContent
    Close fileNum

    MsgBox "Header file generated successfully!", vbInformation

End Sub


