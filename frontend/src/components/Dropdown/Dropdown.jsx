import {useState} from 'react';
import style_arrow from "../Icons/style_icons.module.css";
import Arrow from "../../assets/vector_arrow.svg";
import style_dropdown from './style_dropdown.module.css'

import Dropdown from 'react-dropdown';
import style_accordion from "../Accordion/style_accordion.module.css";


export default function Dropdown({elementList}) {
    const [open, setOpen] = useState(condition);

    const clickHandler = () => {
        setOpen (!open);
        console.log(open);
    }

    return (
        <div className={style_accordion.accordionItem} >
            <div className={style_accordion.accordionHeader} onClick={clickHandler}>
                <div>
                    {title}
                </div>
                <div>
                    <img className={style_arrow.vectorArrow} src={Arrow} alt="arrow">
                    </img>

                </div>
            </div>
            <span className={`${style_accordion.accordionCollapse} ${open ? style_accordion.open : ''}`}>
                <Dropdown>
                     
                </Dropdown>
            </span>
        </div>
    )


}