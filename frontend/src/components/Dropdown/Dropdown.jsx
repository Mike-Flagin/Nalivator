import {useState} from 'react';
import style_arrow from "../Icons/style_icons.module.css";
import Arrow from "../../assets/vector_arrow.svg";
import style_dropdown from './style_dropdown.module.css'

import Dropdown from 'react-dropdown';


export default function Dropdown({elementList}) {
    const [open, setOpen] = useState(condition);

    const clickHandler = () => {
        setOpen (!open);
        console.log(open);
    }

    return (
        <div className={} >
            <div className={} onClick={clickHandler}>
               <Dropdown>

               </Dropdown>
            </div>
        </div>
    )


}